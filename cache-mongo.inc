<?php

require_once dirname(__FILE__) . '/../mongodb/mongodb.module';

/**
 * MonogoDB cache implementation.
 *
 * This is Drupal's default cache implementation. It uses the MongoDB to store
 * cached data. Each cache bin corresponds to a collection by the same name.
 */
class DrupalMongoCache implements DrupalCacheInterface {
  protected $bin;

  function __construct($bin) {
    // Create a little namespace prefix for the collections.
    if (strpos($bin, 'cache') !== 0) {
      $bin = 'cache_'. $bin;
    }
    $this->bin = $bin;
  }

  function get($cid) {
    $collection = mongodb_collection($this->bin);
    // Garbage collection necessary when enforcing a minimum cache lifetime.
    $this->garbageCollection($this->bin);
    $cache = $collection->findOne(array('_id' => (string)$cid));
    return $this->prepareItem($cache);
  }

  function getMultiple(&$cids) {
    $collection = mongodb_collection($this->bin);
    // Garbage collection necessary when enforcing a minimum cache lifetime.
    $this->garbageCollection($this->bin);
    $find = array();
    $find['_id']['$in'] = array_map('strval', $cids);
    $result = $collection->find($find);
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

  /**
   * Garbage collection for get() and getMultiple().
   *
   * @param $bin
   *   The bin being requested.
   */
  function garbageCollection() {
    $collection = mongodb_collection($this->bin);

    // Garbage collection necessary when enforcing a minimum cache lifetime.
    $cache_flush = variable_get('cache_flush_' . $this->bin, 0);
    if ($cache_flush && ($cache_flush + variable_get('cache_lifetime', 0) <= REQUEST_TIME)) {
      // Reset the variable immediately to prevent a meltdown in heavy load situations.
      variable_set('cache_flush_' . $this->bin, 0);
      // Time to flush old cache data


      $find = array('expire' => array('$lte' => $cache_flush, '$ne' => CACHE_PERMANENT));
      $collection->remove($find);
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

    if (!$cache || !isset($cache['data'])) {
      return FALSE;
    }
    unset($cache['_id']);
    $cache = (object)$cache;
    // If the data is permanent or we are not enforcing a minimum cache lifetime
    // always return the cached data.
    if ($cache->expire == CACHE_PERMANENT || !variable_get('cache_lifetime', 0)) {
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
    if ($cache->tags && cache_tags()->isExpired($cache->checksum, $cache->tags)) {
      return FALSE;
    }

    if ($cache->serialized) {
      $cache->data = unserialize($cache->data);
    }

    return $cache;
  }

  function set($cid, $data, $expire = CACHE_PERMANENT, $tags = array()) {
    $scalar = is_scalar($data);
    $entry = array(
      '_id' => (string)$cid,
      'cid' => (string)$cid,
      'created' => REQUEST_TIME,
      'expire' => $expire,
      'serialized' => !$scalar,
      'data' => $scalar ? $data : serialize($data),
      'tags' => $tags,
      'checksum' => cache_tags()->checksum($tags),
    );

    $collection = mongodb_collection($this->bin);

    $collection->save($entry);
  }

  function delete($cid) {
    mongodb_collection($this->bin)->remove(array('_id' => (string)$cid));
  }

  function deleteMultiple(Array $cids) {
    // Delete in chunks when a large array is passed.
    do {
      $find = array('cid' => array('$in' => array_map('strval', array_splice($cids, 0, 1000))));
      mongodb_collection($this->bin)->remove($find);
    }
    while (count($cids));
  }

  function deletePrefix($prefix) {
    mongodb_collection($this->bin)->remove(array('_id' => new MongoRegex('/'. preg_quote($prefix, '/') .'/')));
  }

  function flush() {
    mongodb_collection($this->bin)->remove();
  }

  function expire() {
    if (variable_get('cache_lifetime', 0)) {
      global $user;
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

	mongodb_collection($this->bin)->remove(array('expire' => array('$ne' => CACHE_PERMANENT, '$lte' => REQUEST_TIME)));
	variable_set('cache_flush_' . $this->bin, 0);
      }
    }
    else {
      // No minimum cache lifetime, flush all temporary cache entries now.
      mongodb_collection($this->bin)->remove(array('expire' => array('$ne' => CACHE_PERMANENT, '$lte' => REQUEST_TIME)));
    }
  }

  function clear($cid = NULL, $wildcard = FALSE) {
    if (empty($cid)) {
      $this->expire();
    }
    else {
      if ($wildcard) {
        if ($cid == '*') {
          $this->flush();
        }
        else {
          $this->deletePrefix($cid);
        }
      }
      elseif (is_array($cid)) {
        $this->deleteMultiple($cid);
      }
      else {
        $this->delete($cid);
      }
    }
  }

  function isEmpty() {
    return !mongodb_collection($this->bin)->findOne();
  }
}

class DrupalMongoCacheTags extends DrupalCacheTags implements DrupalCacheTagsInterface {
  protected $tag_cache = array();

  function expire($tags) {
    foreach ($this->flatten($tags) as $tag) {
      unset($this->tag_cache[$tag]);
      mongodb_collection('cache_tags')->update(array('_id' => $tag), array('$inc' => array('invalidations' => 1)), array('upsert' => TRUE));
    }
  }

  function checksum($tags) {
    $checksum = 0;
    $query_tags = array();

    foreach ($this->flatten($tags) as $tag) {
      if (isset($this->tag_cache[$tag])) {
        $checksum += $this->tag_cache[$tag];
      }
      else {
        $query_tags[] = $tag;
      }
    }
    if ($query_tags) {
      foreach (mongodb_collection('cache_tags')->find(array('_id' => array('$in' => $query_tags))) as $db_tag) {
	$this->tag_cache[$db_tag['_id']] = $db_tag['invalidations'];
	$checksum += $db_tag['invalidations'];
      }
    }

    return $checksum;
  }
}
