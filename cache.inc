<?php

/**
 * Get the cache object for a cache bin.
 *
 * By default, this returns an instance of the DrupalDatabaseCache class.
 * Classes implementing DrupalCacheInterface can register themselves both as a
 * default implementation and for specific bins.
 *
 * @see DrupalCacheInterface
 *
 * @param $bin
 *   The cache bin for which the cache object should be returned.
 * @return DrupalCacheInterface
 *   The cache object associated with the specified bin.
 */
function _cache_get_object($bin) {
  // We do not use drupal_static() here because we do not want to change the
  // storage of a cache bin mid-request.
  static $cache_objects;
  if (!isset($cache_objects[$bin])) {
    $class = variable_get('cache_class_' . $bin);
    if (!isset($class)) {
      $class = variable_get('cache_default_class', 'DrupalDatabaseCache');
    }
    $cache_objects[$bin] = new $class($bin);
  }
  return $cache_objects[$bin];
}

/**
 * Return data from the persistent cache
 *
 * Data may be stored as either plain text or as serialized data. cache_get
 * will automatically return unserialized objects and arrays.
 *
 * @param $cid
 *   The cache ID of the data to retrieve.
 * @param $bin
 *   The cache bin to store the data in. Valid core values are 'cache_block',
 *   'cache_bootstrap', 'cache_field', 'cache_filter', 'cache_form',
 *   'cache_menu', 'cache_page', 'cache_path', 'cache_update' or 'cache' for
 *   the default cache.
 *
 * @return
 *   The cache or FALSE on failure.
 */
function cache_get($cid, $bin = 'cache') {
  $cache = _cache_get_object($bin)->get($cid);
  // Add cache tags to this request that was added for this cache entry.
  if (!empty($cache->request_tags)) {
    cache_add_request_tags($cache->request_tags);
  }
  return $cache;
}

/**
 * Return data from the persistent cache when given an array of cache IDs.
 *
 * @param $cids
 *   An array of cache IDs for the data to retrieve. This is passed by
 *   reference, and will have the IDs successfully returned from cache removed.
 * @param $bin
 *   The cache bin where the data is stored.
 * @return
 *   An array of the items successfully returned from cache indexed by cid.
 */
function cache_get_multiple(array &$cids, $bin = 'cache') {
  return _cache_get_object($bin)->getMultiple($cids);
}

/**
 * Store data in the persistent cache.
 *
 * The persistent cache is split up into several cache bins. In the default
 * cache implementation, each cache bin corresponds to a database table by the
 * same name. Other implementations might want to store several bins in data
 * structures that get flushed together. While it is not a problem for most
 * cache bins if the entries in them are flushed before their expire time, some
 * might break functionality or are extremely expensive to recalculate. These
 * will be marked with a (*). The other bins expired automatically by core.
 * Contributed modules can add additional bins and get them expired
 * automatically by implementing hook_flush_caches().
 *
 *  - cache: Generic cache storage bin (used for variables, theme registry,
 *  locale date, list of simpletest tests etc).
 *
 *  - cache_block: Stores the content of various blocks.
 *
 *  - cache field: Stores the field data belonging to a given object.
 *
 *  - cache_filter: Stores filtered pieces of content.
 *
 *  - cache_form(*): Stores multistep forms. Flushing this bin means that some
 *  forms displayed to users lose their state and the data already submitted
 *  to them.
 *
 *  - cache_menu: Stores the structure of visible navigation menus per page.
 *
 *  - cache_page: Stores generated pages for anonymous users. It is flushed
 *  very often, whenever a page changes, at least for every ode and comment
 *  submission. This is the only bin affected by the page cache setting on
 *  the administrator panel.
 *
 *  - cache path: Stores the system paths that have an alias.
 *
 *  - cache update(*): Stores available releases. The update server (for
 *  example, drupal.org) needs to produce the relevant XML for every project
 *  installed on the current site. As this is different for (almost) every
 *  site, it's very expensive to recalculate for the update server.
 *
 * The reasons for having several bins are as follows:
 *
 * - smaller bins mean smaller database tables and allow for faster selects and
 *   inserts
 * - we try to put fast changing cache items and rather static ones into
 *   different bins. The effect is that only the fast changing bins will need a
 *   lot of writes to disk. The more static bins will also be better cacheable
 *   with MySQL's query cache.
 *
 * @param $cid
 *   The cache ID of the data to store.
 * @param $data
 *   The data to store in the cache. Complex data types will be automatically
 *   serialized before insertion.
 *   Strings will be stored as plain text and not serialized.
 * @param $bin
 *   The cache bin to store the data in. Valid core values are 'cache_block',
 *   'cache_bootstrap', 'cache_field', 'cache_filter', 'cache_form',
 *   'cache_menu', 'cache_page', 'cache_update' or 'cache' for the default
 *   cache.
 * @param $expire
 *   One of the following values:
 *   - CACHE_PERMANENT: Indicates that the item should never be removed unless
 *     explicitly told to using cache_clear_all() with a cache ID.
 *   - CACHE_TEMPORARY: Indicates that the item should be removed at the next
 *     general cache wipe.
 *   - A Unix timestamp: Indicates that the item should be kept at least until
 *     the given time, after which it behaves like CACHE_TEMPORARY.
 * @param $tags
 *   An array of namespaces and tags like 'array('namespace' => array(1, 2))'
 *   that can be used to invalidate this cache entry.
 * @param $request_tags
 *   An array of namespaces and tags like the $tags parameter. The request will
 *   get tagged whenever this cache entry is fetched. This can be used to
 *   invalidate the page cache with higer granularity, based on what things that
 *   actually are loaded on the page.
 */
function cache_set($cid, $data, $bin = 'cache', $expire = CACHE_PERMANENT, $tags = array(), $request_tags = array()) {
  return _cache_get_object($bin)->set($cid, $data, $expire, $tags, $request_tags);
}

/**
 * Expire data from the cache.
 *
 * If called without arguments, expirable entries will be cleared from the
 * cache_page and cache_block bins.
 *
 * @param $cid
 *   If set, the cache ID to delete. Otherwise, all cache entries that can
 *   expire are deleted.
 *
 * @param $bin
 *   If set, the bin $bin to delete from. Mandatory
 *   argument if $cid is set.
 *
 * @param $wildcard
 *   If $wildcard is TRUE, cache IDs starting with $cid are deleted in
 *   addition to the exact cache ID specified by $cid.  If $wildcard is
 *   TRUE and $cid is '*' then the entire bin $bin is emptied.
 */
function cache_clear_all($cid = NULL, $bin = NULL, $wildcard = FALSE) {
  if (!isset($cid) && !isset($bin)) {
    // Clear the block cache first, so stale data will
    // not end up in the page cache.
    if (module_exists('block')) {
      cache_clear_all(NULL, 'cache_block');
    }
    cache_clear_all(NULL, 'cache_page');
    return;
  }
  return _cache_get_object($bin)->clear($cid, $wildcard);
}

/**
 * Check if a cache bin is empty.
 *
 * A cache bin is considered empty if it does not contain any valid data for any
 * cache ID.
 *
 * @param $bin
 *   The cache bin to check.
 * @return
 *   TRUE if the cache bin specified is empty.
 */
function cache_is_empty($bin) {
  return _cache_get_object($bin)->isEmpty();
}

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
 * Cache entries tagged with any one of these will subsequently return
 * FALSE for cache_get().
 *
 * @param $tags
 *   Associative array of tags, in the same format that is passed to cache_set().
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
      foreach ($values as $value) {
        if (!in_array($value, $stored_tags[$namespace])) {
          $stored_tags[$namespace][] = $value;
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
 * Interface for cache implementations.
 *
 * All cache implementations have to implement this interface.
 * DrupalDatabaseCache provides the default implementation, which can be
 * consulted as an example.
 *
 * To make Drupal use your implementation for a certain cache bin, you have to
 * set a variable with the name of the cache bin as its key and the name of
 * your class as its value. For example, if your implementation of
 * DrupalCacheInterface was called MyCustomCache, the following line would make
 * Drupal use it for the 'cache_page' bin:
 * @code
 *  variable_set('cache_class_cache_page', 'MyCustomCache');
 * @endcode
 *
 * Additionally, you can register your cache implementation to be used by
 * default for all cache bins by setting the variable 'cache_default_class' to
 * the name of your implementation of the DrupalCacheInterface, e.g.
 * @code
 *  variable_set('cache_default_class', 'MyCustomCache');
 * @endcode
 *
 * To implement a completely custom cache bin, use the same variable format:
 * @code
 *  variable_set('cache_class_custom_bin', 'MyCustomCache');
 * @endcode
 * To access your custom cache bin, specify the name of the bin when storing
 * or retrieving cached data:
 * @code
 *  cache_set($cid, $data, 'custom_bin', $expire);
 *  cache_get($cid, 'custom_bin');
 * @endcode
 *
 * @see _cache_get_object()
 * @see DrupalDatabaseCache
 */
interface DrupalCacheInterface {
  /**
   * Constructor.
   *
   * @param $bin
   *   The cache bin for which the object is created.
   */
  function __construct($bin);

  /**
   * Return data from the persistent cache. Data may be stored as either plain
   * text or as serialized data. cache_get will automatically return
   * unserialized objects and arrays.
   *
   * @param $cid
   *   The cache ID of the data to retrieve.
   * @return
   *   The cache or FALSE on failure.
   */
  function get($cid);

  /**
   * Return data from the persistent cache when given an array of cache IDs.
   *
   * @param $cids
   *   An array of cache IDs for the data to retrieve. This is passed by
   *   reference, and will have the IDs successfully returned from cache
   *   removed.
   * @return
   *   An array of the items successfully returned from cache indexed by cid.
   */
   function getMultiple(&$cids);

  /**
   * Store data in the persistent cache.
   *
   * @param $cid
   *   The cache ID of the data to store.
   * @param $data
   *   The data to store in the cache. Complex data types will be automatically
   *   serialized before insertion.
   *   Strings will be stored as plain text and not serialized.
   * @param $expire
   *   One of the following values:
   *   - CACHE_PERMANENT: Indicates that the item should never be removed unless
   *     explicitly told to using cache_clear_all() with a cache ID.
   *   - CACHE_TEMPORARY: Indicates that the item should be removed at the next
   *     general cache wipe.
   *   - A Unix timestamp: Indicates that the item should be kept at least until
   *     the given time, after which it behaves like CACHE_TEMPORARY.
   */
  function set($cid, $data, $expire = CACHE_PERMANENT);


  /**
   * Expire data from the cache. If called without arguments, expirable
   * entries will be cleared from the cache_page and cache_block bins.
   *
   * @param $cid
   *   If set, the cache ID to delete. Otherwise, all cache entries that can
   *   expire are deleted.
   * @param $wildcard
   *   If set to TRUE, the $cid is treated as a substring
   *   to match rather than a complete ID. The match is a right hand
   *   match. If '*' is given as $cid, the bin $bin will be emptied.
   */
  function clear($cid = NULL, $wildcard = FALSE);

  /**
   * Check if a cache bin is empty.
   *
   * A cache bin is considered empty if it does not contain any valid data for
   * any cache ID.
   *
   * @return
   *   TRUE if the cache bin specified is empty.
   */
  function isEmpty();
}

/**
 * Default cache implementation.
 *
 * This is Drupal's default cache implementation. It uses the database to store
 * cached data. Each cache bin corresponds to a database table by the same name.
 */
class DrupalDatabaseCache implements DrupalCacheInterface {
  protected $bin;

  function __construct($bin) {
    $this->bin = $bin;
  }

  function get($cid) {
    $cids = array($cid);
    $cache = $this->getMultiple($cids);
    return reset($cache);
  }

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
   * Garbage collection for get() and getMultiple().
   *
   * @param $bin
   *   The bin being requested.
   */
  protected function garbageCollection() {
    global $user;

    // Garbage collection necessary when enforcing a minimum cache lifetime.
    $cache_flush = variable_get('cache_flush_' . $this->bin, 0);
    if ($cache_flush && ($cache_flush + variable_get('cache_lifetime', 0) <= REQUEST_TIME)) {
      // Reset the variable immediately to prevent a meltdown in heavy load situations.
      variable_set('cache_flush_' . $this->bin, 0);
      // Time to flush old cache data
      db_delete($this->bin)
        ->condition('expire', CACHE_PERMANENT, '<>')
        ->condition('expire', $cache_flush, '<=')
        ->execute();
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
   * @return
   *   The item with data unserialized as appropriate or FALSE if there is no
   *   valid item to load.
   */
  protected function prepareItem($cache) {
    global $user;

    if (!isset($cache->data)) {
      return FALSE;
    }
    // If enforcing a minimum cache lifetime, validate that the data is
    // currently valid for this user before we return it by making sure the cache
    // entry was created before the timestamp in the current session's cache
    // timer. The cache variable is loaded into the $user object by _drupal_session_read()
    // in session.inc. If the data is permanent or we're not enforcing a minimum
    // cache lifetime always return the cached data.
    if ($cache->expire != CACHE_PERMANENT && variable_get('cache_lifetime', 0) && $user->cache > $cache->created) {
      // This cache data is too old and thus not valid for us, ignore it.
      return FALSE;
    }

    // The cache data is invalid if any of its tags have been cleared since.
    if ($cache->tags) {
      $cache->tags = explode(' ', $cache->tags);
      if (!cache_tags()->isValid($cache->checksum, $cache->tags)) {
        return FALSE;
      }
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

  function clear($cid = NULL, $wildcard = FALSE) {
    global $user;

    if (empty($cid)) {
      if (variable_get('cache_lifetime', 0)) {
        // We store the time in the current user's $user->cache variable which
        // will be saved into the sessions bin by _drupal_session_write(). We then
        // simulate that the cache was flushed for this user by not returning
        // cached data that was cached before the timestamp.
        $user->cache = REQUEST_TIME;

        $cache_flush = variable_get('cache_flush_' . $this->bin, 0);
        if ($cache_flush == 0) {
          // This is the first request to clear the cache, start a timer.
          variable_set('cache_flush_' . $this->bin, REQUEST_TIME);
        }
        elseif (REQUEST_TIME > ($cache_flush + variable_get('cache_lifetime', 0))) {
          // Clear the cache for everyone, cache_lifetime seconds have
          // passed since the first request to clear the cache.
          db_delete($this->bin)
            ->condition('expire', CACHE_PERMANENT, '<>')
            ->condition('expire', REQUEST_TIME, '<')
            ->execute();
          variable_set('cache_flush_' . $this->bin, 0);
        }
      }
      else {
        // No minimum cache lifetime, flush all temporary cache entries now.
        db_delete($this->bin)
          ->condition('expire', CACHE_PERMANENT, '<>')
          ->condition('expire', REQUEST_TIME, '<')
          ->execute();
      }
    }
    else {
      if ($wildcard) {
        if ($cid == '*') {
          db_truncate($this->bin)->execute();
        }
        else {
          db_delete($this->bin)
            ->condition('cid', db_like($cid) . '%', 'LIKE')
            ->execute();
        }
      }
      elseif (is_array($cid)) {
        // Delete in chunks when a large array is passed.
        do {
          db_delete($this->bin)
            ->condition('cid', array_splice($cid, 0, 1000), 'IN')
            ->execute();
        }
        while (count($cid));
      }
      else {
        db_delete($this->bin)
          ->condition('cid', $cid)
          ->execute();
      }
    }
  }

  function isEmpty() {
    $this->garbageCollection();
    $query = db_select($this->bin);
    $query->addExpression('1');
    $result = $query->range(0, 1)
      ->execute()
      ->fetchField();
    return empty($result);
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
   *   Associative array of tags, in the same format that is passed to cache_set().
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
   *   Associative array of tags, in the same format that is passed to cache_set().
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
