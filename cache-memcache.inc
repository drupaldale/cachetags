<?php
/**
 * @file
 * Memcache integration.
 */

require_once dirname(__FILE__) . '/../memcache/memcache.inc';

class DrupalMemcacheCacheTagsPlugin extends MemCacheDrupal {

  protected function valid($cid, $cache) {
    // The cache data is invalid if any of its tags have been cleared since.
    if (!empty($cache->tags) && !cache_tags()->isValid($cache->checksum, $cache->tags)) {
      return FALSE;
    }
    return parent::valid($cid, $cache);
  }

  function set($cid, $data, $expire = CACHE_PERMANENT, $tags = array(), $request_tags = array()) {
    $created = time();

    // Create new cache object.
    $cache = new stdClass;
    $cache->cid = $cid;
    $cache->data = is_object($data) ? clone $data : $data;
    $cache->created = $created;
    // Record the previous number of wildcard flushes affecting our cid.
    $cache->flushes = $this->wildcard_flushes($cid);
    if ($expire == CACHE_TEMPORARY) {
      // Convert CACHE_TEMPORARY (-1) into something that will live in memcache
      // until the next flush.
      $cache->expire = REQUEST_TIME + 2591999;
    }
    // Expire time is in seconds if less than 30 days, otherwise is a timestamp.
    else if ($expire != CACHE_PERMANENT && $expire < 2592000) {
      // Expire is expressed in seconds, convert to the proper future timestamp
      // as expected in dmemcache_get().
      $cache->expire = REQUEST_TIME + $expire;
    }
    else {
      $cache->expire = $expire;
    }

    // Add cache tags.
    $cache->tags = $tags;
    $cache->request_tags = $request_tags;
    $cache->checksum = cache_tags()->checksum($tags);

    // We manually track the expire time in $cache->expire.  When the object
    // expires, we only allow one request to rebuild it to avoid cache
    // stampedes. Other requests for the expired object while it is still being
    // rebuilt get the expired object.
    dmemcache_set($cid, $cache, 0, $this->bin, $this->memcache);
  }
}

class DrupalMemcacheCacheTags extends DrupalCacheTags {
  protected $tag_cache = array();

  function __construct() {
    $this->memcache = dmemcache_object('cache_tags');
  }

  function invalidate($tags) {
    $flat_tags = array();
    $invalidated_tags = array();
    foreach (DrupalCacheTags::flatten($tags) as $tag) {
      unset($this->tag_cache[$tag]);
      $flat_tags[] = $tag;
    }
    // Memcache can't do a smart merge or update queries so we iterate a bit.
    $results = dmemcache_get_multi($flat_tags, 'cache_tags', $this->memcache);
    foreach ($results as $tag => $result) {
      $result->invalidations += 1;
      dmemcache_set($tag, $result, 0, 'cache_tags', $this->memcache);
      $invalidated_tags[$tag] = TRUE;
    }
    // Now invalidate all tags that didn't already exist in the cache_tags bin.
    foreach ($flat_tags as $tag) {
      if (!isset($invalidated_tags[$tag])) {
        $object = new stdClass();
        $object->invalidations = 1;
        dmemcache_set($tag, $object, 0, 'cache_tags', $this->memcache);
      }
    }
  }

  function checksum($tags) {
    $checksum = 0;
    $flat_tags = array();
    foreach (DrupalCacheTags::flatten($tags) as $tag) {
      if (isset($this->tag_cache[$tag])) {
        $checksum += $this->tag_cache[$tag];
      }
      else {
        $flat_tags[] = $tag;
      }
    }
    if (!empty($flat_tags)) {
      $results = dmemcache_get_multi($flat_tags, 'cache_tags', $this->memcache);
      foreach ($results as $tag => $result) {
        $this->tag_cache[$tag] = $result->invalidations;
        $checksum += $result->invalidations;
      }
    }
    return $checksum;
  }
}
