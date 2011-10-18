Cache Tags

An experimental project to associate cache entries with metadata, which then
enables developers to clear caches based on that metadata.

Setup (D7 version):

*  If starting from scratch, (otherwise must have a working D7 install)

     git clone --branch 7.x http://git.drupal.org/project/drupal.git

*  Install this project in sites/all/modules/cachetags.

*  Apply the core patch:

     git apply sites/all/modules/cachetags/cache_tags.patch

*  If starting from scratch, install the site.

*  If starting with an existing install, enable the cachetags_sql module.

*  If using Mongo storage, install http://drupal.org/project/mongodb, (must be
   in sites/all/modules/mongodb) then add this to settings.php:

     $conf['cache_backends'] = array('sites/all/modules/cachetags/cache-mongo.inc');
     $conf['cache_default_class'] = 'DrupalMongoCache';
     $conf['cache_tags_class'] = 'DrupalMongoCacheTags';

*  To test, run the benchmark script:

     ./sites/all/modules/cachetags/benchmark.sh

See http://drupal.org/node/636454 for ongoing core development.


Cheers!
Carlos Rodriguez "carlos8f" <http://drupal.org/user/454578>
