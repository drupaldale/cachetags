CacheTags
---------
An experimental project to associate cache entries with metadata, which then
enables developers to clear caches based on that metadata.


Installation
------------
* Copy the module to the site's standard module location, e.g.
  sites/all/modules/contrib.

* Apply the core patch, which modifies cache_set() to accept $tags:

    git apply sites/all/modules/contrib/cachetags/cachetags.patch

* If using the default database-driven cache, enable the cachetags_sql module
  and add this to the site's settings.php:

    $conf['cache_backends'] = array('sites/all/modules/contrib/cachetags/cache-db.inc');
    $conf['cache_default_class'] = 'DrupalDatabaseCacheTagsPlugin';
    $conf['cache_tags_class'] = 'DrupalDatabaseCacheTags';

* If using memcached, install http://drupal.org/project/memcache (v7.x-1.0 or
  newer) and add this to the site's settings.php:

    $conf['cache_backends'] = array('sites/all/modules/contrib/cachetags/cache-memcache.inc');
    $conf['cache_default_class'] = 'DrupalMemcacheCacheTagsPlugin';
    $conf['cache_tags_class'] = 'DrupalMemcacheCacheTags';

* If using MongoDB, install http://drupal.org/project/mongodb and add this to
  the site's settings.php:

    $conf['cache_backends'] = array('sites/all/modules/contrib/cachetags/cache-mongo.inc');
    $conf['cache_default_class'] = 'DrupalMongoCacheTagsPlugin';
    $conf['cache_tags_class'] = 'DrupalMongoCacheTags';

* To test, run the benchmark script:

    ./sites/all/modules/contrib/cachetags/benchmark.sh

See http://drupal.org/node/636454 for on-going core development.


Credits / Contact
------------------------------------------------------------------------------
Currently maintained by Dick Olson [1] and Damien McKenna [2]. All initial
development was by Carlos "carlos8f" Rodriguez [3].

Continued development sponsored by Al Jazeera [4] and Mediacurrent [5].

The best way to contact the authors is to submit an issue, be it a support
request, a feature request or a bug report, in the project issue queue:
  http://drupal.org/project/issues/metatag


References
------------------------------------------------------------------------------
1: http://drupal.org/user/239911
2: http://drupal.org/user/108450
3: http://drupal.org/user/454578
4: http://www.aljazeera.com/
5: http://www.mediacurrent.com/
