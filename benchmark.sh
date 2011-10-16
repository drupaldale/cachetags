#!/usr/bin/php
<?php

$_SERVER['REMOTE_ADDR'] = '127.0.0.1';
define('DRUPAL_ROOT', getcwd());
require_once DRUPAL_ROOT . '/includes/bootstrap.inc';
drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);

timer_start('set');
for ($i = 0; $i < 10000; $i++) {
  cache_set("test-$i", NULL, 'cache', CACHE_PERMANENT, array('user' => array(1, 2, 3, 4), 'node' => array(1, 2, 3, 4)));
}
timer_stop('set');

timer_start('get');
for ($i = 0; $i < 10000; $i++) {
  cache_get("test-$i");
}
timer_stop('get');

timer_start('clear 1');
cache_expire_tagged(array('node' => array(3)), 'cache');
timer_stop('clear 1');

timer_start('clear multi');
cache_expire_tagged(array('user' => array(1, 2, 3, 4), 'node' => array(1, 2, 4)), 'cache');
//cache_clear_all('test', 'cache', TRUE);
timer_stop('clear multi');

unset($GLOBALS['timers']['page']);
var_dump($GLOBALS['timers']);
