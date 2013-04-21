<?php

require_once DRUPAL_ROOT . '/sites/all/modules/varnish/varnish.module';
require_once DRUPAL_ROOT . '/sites/all/modules/varnish/varnish.cache.inc';

class DrupalVarnishCacheTagsPlugin extends VarnishCache {
  function set($cid, $data, $expire = CACHE_PERMANENT, $tags = array(), $request_tags = array()) {
    if (!empty($tags)) {
      drupal_add_http_header('X-Cache-Tags', implode(',', DrupalCacheTags::flatten($tags)));
    }
  }

  function invalidate($tags = array()) {
    $commands = array();
    $host = _varnish_get_host();
    foreach (DrupalCacheTags::flatten($tags) as $tag) {
      $commands[] = 'purge req.http.host ~ ' . $host . ' && obj.http.X-Cache-Tags ~ ' . $tag;
    }
    _varnish_terminal_run($commands);
  }
}
