<?php
/**
 * @file
 * Database cache integration.
 */

require_once dirname(__FILE__) . '/../varnish/varnish.module';
require_once dirname(__FILE__) . '/../varnish/varnish.cache.inc';

class DrupalVarnishCacheTagsPlugin extends VarnishCache {
  function set($cid, $data, $expire = CACHE_PERMANENT, $tags = array(), $request_tags = array()) {
    if (!empty($tags)) {
      drupal_add_http_header('X-Cache-Tags', implode(',', DrupalCacheTags::flatten($tags)));
    }
  }

  function invalidate($tags = array()) {
    $version = floatval(variable_get('varnish_version', 2.1));
    $command = $version >= 3 ? "ban" : "purge";

    $commands = array();
    $host = _varnish_get_host();
    foreach (DrupalCacheTags::flatten($tags) as $tag) {
      $commands[] = "$command req.http.host ~ $host && obj.http.X-Cache-Tags ~ $tag";
    }
    _varnish_terminal_run($commands);
  }
}
