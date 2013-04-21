<?php
/**
 * @file
 * CacheTags cache plugin for Panels/CTools.
 */

$plugin = array(
  'title' => t('Cache Tags'),
  'description' => t('This plugin sets the cache with tags that each pane decide.'),
  'cache get' => 'cachetags_cache_get',
  'cache set' => 'cachetags_cache_set',
  'cache clear' => 'cachetags_cache_clear',
  'settings form' => 'cachetags_settings_form',
  'settings form submit' => 'cachetags_settings_form_submit',
  'defaults' => array(
    'cache_key' => '',
  ),
);

/**
 * Get cached content.
 */
function cachetags_cache_get($conf, $display, $args, $contexts, $pane = NULL) {
  $cid = cachetags_cid($conf, $display, $args, $contexts, $pane);
  $cache = cache_get($cid, 'cache_tags_panels');
  if (!$cache) {
    return FALSE;
  }
  return $cache->data;
}

/**
 * Set cached content.
 */
function cachetags_cache_set($conf, $content, $display, $args = array(), $contexts, $pane = NULL) {
  $cid = cachetags_cid($conf, $display, $args, $contexts, $pane);

  $tags = empty($content->content->cache_tags) ? array() : $content->content->cache_tags;
  $request_tags = empty($content->content->request_tags) ? array() : $content->content->request_tags;

  cache_set($cid, $content, 'cache_tags_panels', CACHE_PERMANENT, $tags, $request_tags);
}

/**
 * Clear cached content.
 */
function cachetags_cache_clear($display) {
  if (is_numeric($display->did) && $display->did) {
    $cid = $display->did;
  }
  else {
    $cid = $display->cache_key;
  }
  if (isset($display->clear_pane)) {
    $cid .= ':' . $display->clear_pane->pid;
  }
  cache_clear_all($cid, 'cache_tags_panels', TRUE);
}

/**
 * Figure out an id for our cache based upon input and settings.
 */
function cachetags_cid($conf, $display, $args, $contexts, $pane) {
  // Add basic keys.
  if ($display->did && is_numeric($display->did)) {
    $cid = $display->did;
  }
  else {
    $cid = $conf['cache_key'];
  }
  if ($pane) {
    $cid .= ':' . $pane->pid;
  }
  // Add context keys.
  if (!is_array($contexts)) {
    $contexts = array($contexts);
  }
  foreach ($contexts as $context) {
    if (is_string($context->argument) && !empty($context->argument)) {
      $cid .= ':' . $context->argument;
    }
  }
  // Add argument keys.
  foreach ($args as $arg) {
    if (is_string($arg) && !empty($arg)) {
      $cid .= ":$arg";
    }
  }
  // Add language key.
  $cid .= ':' . $GLOBALS['language']->language;
  // Add pager keys.
  if (!empty($_GET['page'])) {
    $pager_ids = explode(',', $_GET['page']);
    if (!empty($pane->configuration['use_pager']) && isset($pager_ids[$pane->configuration['pager_id']])) {
      $cid .= ':pager:' . check_plain($pager_ids[$pane->configuration['pager_id']]);
    }
  }
  // Admins needs an additional key.
  // TODO: Consider making this configurable to instead cache per role. Some
  // configurations doesn't need to make this distinction.
  if (user_access('view pane admin links')) {
    $cid .= ':admin';
  }
  return $cid;
}

function cachetags_settings_form(&$conf, $display, $pid) {
  // This is a hack to make sure that we have the cache key for the panel
  // at all times when we need to invalidate the cache. When all caching options
  // are implemented in Panels for Drupal 7, we should be able to remove this.
  $form['cache_key'] = array(
    '#type' => 'value',
    '#value' => $display->cache_key,
  );
  return $form;
}
