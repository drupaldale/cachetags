<?php

interface DrupalCacheTagsInterface {
  function __construct();

  function invalidate($tags);

  function checksum($tags);

  function isValid($checksum, $tags);
}

abstract class DrupalCacheTags {
  function __construct() {

  }

  function flatten($tags) {
    if (isset($tags[0])) return $tags;
    $flat_tags = array();
    foreach ($tags as $namespace => $values) {
      foreach ($values as $value) {
        $flat_tags[] = "$namespace:$value";
      }
    }
    return $flat_tags;
  }

  function isValid($checksum, $tags) {
    return $checksum == $this->checksum($tags);
  }
}

function cache_tags() {
  $object = &drupal_static(__FUNCTION__);
  if (!isset($object)) {
    $class = variable_get('cache_tags_class', 'DrupalDatabaseCacheTags');
    $object = new $class;
  }
  return $object;
}

function cache_invalidate($tags) {
  return cache_tags()->invalidate($tags);
}
