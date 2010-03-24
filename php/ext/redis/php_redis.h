#ifndef PHP_REDIS_H
#define PHP_REDIS_H 1

#define PHP_REDIS_VERSION "1.0"
#define PHP_REDIS_EXTNAME "redis"

PHP_FUNCTION(hello_world);

extern zend_module_entry redis_module_entry;
#define phpext_redis_ptr &redis_module_entry

#endif
