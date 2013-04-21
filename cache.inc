<?php
/**
 * @file
 * Core cachetags cache engine.
 */

/**
 * Factory for instantiating and statically caching the cache tags instance.
 *
 * By default, this returns an instance of the DrupalDatabaseCacheTags class.
 * To override this, you must set a variable (either with variable_set() or
 * in settings.php via $conf):
 * @code
 *  variable_set('cache_tags_class', 'MyCustomCacheTags');
 * @endcode
 *
 * @see DrupalCacheTagsInterface
 *
 * @return DrupalCacheTagsInterface
 *   The cache tags object.
 */
function cache_tags() {
  $object = &drupal_static(__FUNCTION__);
  if (!isset($object)) {
    $class = variable_get('cache_tags_class', 'DrupalDatabaseCacheTags');
    $object = new $class;
  }
  return $object;
}

/**
 * Invalidate each tag in the $tags array.
 *
 * Procedural (convenience) wrapper for the cache_tags()->invalidate() method.
 * Cache entries tagged with any one of these will subsequently return FALSE
 * for cache_get().
 *
 * @param $tags
 *   Associative array of tags, in the same format that is passed to
 *   cache_set().
 *
 * @see cache_set()
 */
function cache_invalidate($tags) {
  cache_tags()->invalidate($tags);
  $bins = variable_get('cache_external_bins', array());
  foreach ($bins as $bin) {
    _cache_get_object($bin)->invalidate($tags);
  }
}

/**
 * Helper function to add cache tags for the current request.
 */
function cache_add_request_tags($tags = array()) {
  $stored_tags = &drupal_static(__FUNCTION__, array());
  if (!empty($tags)) {
    // Merge cache tags into appropriate namespace.
    foreach ($tags as $namespace => $values) {
      if (!isset($stored_tags[$namespace])) {
        $stored_tags[$namespace] = array();
      }
      if (is_array($values) && !empty($values)) {
        foreach ($values as $value) {
          if (!in_array($value, $stored_tags[$namespace])) {
            $stored_tags[$namespace][] = $value;
          }
        }
      }
    }
  }
  return $stored_tags;
}

/**
 * Helper function to get cache tags for the current page.
 */
function cache_get_request_tags() {
  return cache_add_request_tags();
}

/**
 * Database cache implementation.
 */
class CacheTagsDatabaseCache extends DrupalDatabaseCache {

  function getMultiple(&$cids) {
    try {
      // Garbage collection necessary when enforcing a minimum cache lifetime.
      $this->garbageCollection($this->bin);

      // When serving cached pages, the overhead of using db_select() was found
      // to add around 30% overhead to the request. Since $this->bin is a
      // variable, this means the call to db_query() here uses a concatenated
      // string. This is highly discouraged under any other circumstances, and
      // is used here only due to the performance overhead we would incur
      // otherwise. When serving an uncached page, the overhead of using
      // db_select() is a much smaller proportion of the request.
      $result = db_query('SELECT cid, data, created, expire, serialized, tags, request_tags, checksum FROM {' . db_escape_table($this->bin) . '} WHERE cid IN (:cids)', array(':cids' => $cids));
      $cache = array();
      foreach ($result as $item) {
        $item = $this->prepareItem($item);
        if ($item) {
          $cache[$item->cid] = $item;
        }
      }
      $cids = array_diff($cids, array_keys($cache));
      return $cache;
    }
    catch (Exception $e) {
      // If the database is never going to be available, cache requests should
      // return FALSE in order to allow exception handling to occur.
      return array();
    }
  }

  /**
   * Prepare a cached item.
   *
   * Checks that items are either permanent or did not expire, and unserializes
   * data as appropriate.
   *
   * @param $cache
   *   An item loaded from cache_get() or cache_get_multiple().
   *
   * @return
   *   The item with data unserialized as appropriate or FALSE if there is no
   *   valid item to load.
   */
  protected function prepareItem($cache) {
    global $user;

    if (!isset($cache->data)) {
      return FALSE;
    }
    // If the cached data is temporary and subject to a per-user minimum
    // lifetime, compare the cache entry timestamp with the user session
    // cache_expiration timestamp. If the cache entry is too old, ignore it.
    if ($cache->expire != CACHE_PERMANENT && variable_get('cache_lifetime', 0) && isset($_SESSION['cache_expiration'][$this->bin]) && $_SESSION['cache_expiration'][$this->bin] > $cache->created) {
      // Ignore cache data that is too old and thus not valid for this user.
      return FALSE;
    }

    // The cache data is invalid if any of its tags have been cleared since.
    if ($cache->tags) {
      $cache->tags = explode(' ', $cache->tags);
      if (!cache_tags()->isValid($cache->checksum, $cache->tags)) {
        return FALSE;
      }
    }

    if ($cache->request_tags) {
      $cache->request_tags = explode(' ', $cache->request_tags);
    }

    if ($cache->serialized) {
      $cache->data = unserialize($cache->data);
    }

    return $cache;
  }

  function set($cid, $data, $expire = CACHE_PERMANENT, $tags = array(), $request_tags = array()) {
    $fields = array(
      'serialized' => 0,
      'created' => REQUEST_TIME,
      'expire' => $expire,
      'tags' => implode(' ', DrupalCacheTags::flatten($tags)),
      'request_tags' => implode(' ', DrupalCacheTags::flatten($request_tags)),
      'checksum' => cache_tags()->checksum($tags),
    );
    if (!is_string($data)) {
      $fields['data'] = serialize($data);
      $fields['serialized'] = 1;
    }
    else {
      $fields['data'] = $data;
      $fields['serialized'] = 0;
    }

    try {
      db_merge($this->bin)
        ->key(array('cid' => $cid))
        ->fields($fields)
        ->execute();
    }
    catch (Exception $e) {
      // The database may not be available, so we'll ignore cache_set requests.
    }
  }
}

/**
 * Interface for cache tag implementations.
 *
 * Cache tags and their corresponding invalidation counters are stored
 * independently from the cache itself. Cache tag storage is stored on a
 * site-wide level (as opposed to per-bin) so only one cache tag storage engine
 * can be used at one time.
 *
 * To make Drupal use your cache tags implementation, you must set a variable
 * (either with variable_set() or in settings.php via $conf):
 * @code
 *  variable_set('cache_tags_class', 'MyCustomCacheTags');
 * @endcode
 *
 * @see cache_tags()
 * @see DrupalDatabaseCacheTags
 */
interface DrupalCacheTagsInterface {
  /**
   * Invalidate each tag in the $tags array.
   *
   * Cache entries tagged with any one of these will subsequently return
   * FALSE for cache_get().
   *
   * @param $tags
   *   Associative array of tags, in the same format that is passed to
   *   cache_set().
   *
   * @see cache_set()
   */
  function invalidate($tags);

  /**
   * Calculate the sum of invalidations of the given tags.
   *
   * @param $tags
   *   Associative array of tags to calculate checksum for.
   *
   * @return
   *   Integer representing the sum of invalidations of the given tags.
   */
  function checksum($tags);

  /**
   * Determine if the checksum is current and valid.
   *
   * @param $checksum
   *   A checksum to test against.
   * @param $tags
   *   Associative array of tags, in the same format that is passed to
   *   cache_set().
   *
   * @return
   *   Boolean TRUE if $checksum is current for the given tags, FALSE otherwise.
   */
  function isValid($checksum, $tags);
}

/**
 * Base abstract class for cache tags implementations.
 *
 * Provides default implementations and helper methods.
 */
abstract class DrupalCacheTags implements DrupalCacheTagsInterface {
  function isValid($checksum, $tags) {
    return $checksum == $this->checksum($tags);
  }

  /**
   * Flatten a tags array into a numeric array suitable for string storage.
   *
   * @param $tags
   *   Associative array of tags to flatten.
   *
   * @return
   *   Numeric array of flattened tag identifiers.
   */
  static function flatten($tags) {
    if (isset($tags[0])) return $tags;
    $flat_tags = array();
    foreach ($tags as $namespace => $values) {
      if (is_array($values)) {
        foreach ($values as $value) {
          $flat_tags[] = "$namespace:$value";
        }
      }
      else {
        $flat_tags[] = "$namespace:$values";
      }
    }
    return $flat_tags;
  }
}

/**
 * Default cache tags implementation, using the database.
 */
class DrupalDatabaseCacheTags extends DrupalCacheTags {
  protected $tag_cache = array();

  function invalidate($tags) {
    foreach (DrupalCacheTags::flatten($tags) as $tag) {
      unset($this->tag_cache[$tag]);
      db_merge('cache_tags')
        ->key(array('tag' => $tag))
        ->fields(array('invalidations' => 1))
        ->expression('invalidations', 'invalidations + 1')
        ->execute();
    }
  }

  function checksum($tags) {
    $checksum = 0;
    $query_tags = array();

    foreach (DrupalCacheTags::flatten($tags) as $tag) {
      if (isset($this->tag_cache[$tag])) {
        $checksum += $this->tag_cache[$tag];
      }
      else {
        $query_tags[] = $tag;
      }
    }
    if ($query_tags) {
      if ($db_tags = db_query('SELECT tag, invalidations FROM {cache_tags} WHERE tag IN (:tags)', array(':tags' => $query_tags))->fetchAllKeyed()) {
        $this->tag_cache = array_merge($this->tag_cache, $db_tags);
        $checksum += array_sum($db_tags);
      }
    }

    return $checksum;
  }
}
