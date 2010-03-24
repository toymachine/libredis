PHP_ARG_ENABLE(redis, whether to enable Redis support,
[ --enable-redis   Enable Redis support])

if test "$PHP_REDIS" = "yes"; then
  AC_DEFINE(HAVE_REDIS, 1, [Whether you have Redis])
  PHP_NEW_EXTENSION(redis, php_redis.c, $ext_shared)
fi
