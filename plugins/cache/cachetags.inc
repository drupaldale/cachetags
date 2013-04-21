<?php

$plugin = array(
  'title' => t('Cache Tags'),
  'description' => t('This plugin sets the cache with tags that each pane decide.'),
  'cache get' => 'cachetags_cache_get',
  'cache set' => 'cachetags_cache_set',
  'cache clear' => 'cachetags_cache_clear',
  'settings form' => 'cachetags_settings_form',
  'settings form submit' => 'cachetags_settings_form_submit',
  'defaults' => array(
    'granularity' => 'context',
    'language' => 1,
    'cache_key' => '',
  ),
);

/**
 * Get cached content.
 */
function cachetags_cache_get($conf, $display, $args, $contexts, $pane = NULL) {
  $cid = cachetags_cid($conf, $display, $args, $contexts, $pane);
  $cache = cache_get($cid, 'cache_cachetags_panels');
  if (!$cache) {
    return FALSE;
  }
  return $cache->data;
}

/**
 * Set cached content.
 */
function cachetags_cache_set($conf, $content, $display, $args, $contexts, $pane = NULL) {
  $cid = cachetags_cid($conf, $display, $args, $contexts, $pane);

  $tags = empty($content->content->cache_tags) ? array() : $content->content->cache_tags;
  $request_tags = empty($content->content->request_tags) ? array() : $content->content->request_tags;

  cache_set($cid, $content, 'cache_cachetags_panels', CACHE_PERMANENT, $tags, $request_tags);
}

/**
 * Clear cached content.
 */
function cachetags_cache_clear($display) {
  if (is_numeric($display->did) && $display->did) {
    $cid = 'display:' . $display->did;
  }
  else {
    $cid = 'display:' . $display->cache_key;
  }
  if (isset($display->owner->name)) {
    $cid .= ':' . $display->owner->name;
  }
  if (isset($display->clear_pane)) {
    $cid .= ':' . $display->clear_pane->pid;
  }
  cache_clear_all($cid, 'cache_cachetags_panels', TRUE);
}

/**
 * Figure out an id for our cache based upon input and settings.
 */
function cachetags_cid($conf, $display, $args, $contexts, $pane) {
  if ($display->did && is_numeric($display->did)) {
    $cid = $display->did;
  }
  else {
    $cid = $conf['cache_key'];
  }
  if (isset($display->owner->name)) {
    $cid .= ':' . $display->owner->name;
  }
  if ($pane) {
    $cid .= ':' . $pane->pid;
  }

  switch ($conf['granularity']) {
    case 'args':
      foreach ($args as $arg) {
        if (is_string($arg)) {
          $cid .= ':' . $arg;
        }
      }
      break;

    case 'context':
      if (!is_array($contexts)) {
        $contexts = array($contexts);
      }
      foreach ($contexts as $context) {
        if (isset($context->argument)) {
          $cid .= ':' . $context->argument;
        }
      }
  }

  if (user_access('view pane admin links')) {
    $cid .= ':admin';
  }
  if ($conf['language']) {
    global $language;
    $cid .= ':' . $language->language;
  }
  return $cid;
}

function cachetags_settings_form(&$conf, $display, $pid) {
  $form['granularity'] = array(
    '#title' => t('Granularity'),
    '#type' => 'select',
    '#options' => array(
      'none' => t('None'),
      'args' => t('Arguments'),
      'context' => t('Context'),
    ),
    '#description' => t('If "arguments" are selected, this content will be cached per individual argument to the entire display; if "contexts" are selected, this content will be cached per unique context in the pane or display; if "neither" there will be only one cache for this pane.'),
    '#default_value' => $conf['granularity'],
  );
  $form['language'] = array(
    '#title' => t('Cache per language'),
    '#type' => 'checkbox',
    '#default_value' => $conf['language'],
    '#description' => t('Select this if you want to cache content per language'),
  );

  // This is a hack to make sure that we have the cache key for the panel
  // at all times when we need to invalidate the cache. When all caching options
  // are implemented in Panels for Drupal 7, we should be able to remove this.
  $form['cache_key'] = array(
    '#type' => 'value',
    '#value' => $display->cache_key,
  );
  return $form;
}
