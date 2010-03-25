dnl $Id$
dnl config.m4 for extension redis

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

PHP_ARG_WITH(redis, for redis support,
[  --with-redis             Include redis support])

dnl Otherwise use enable:

dnl PHP_ARG_ENABLE(redis, whether to enable redis support,
dnl Make sure that the comment is aligned:
dnl [  --enable-redis           Enable redis support])

if test "$PHP_REDIS" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-redis -> check with-path
  SEARCH_PATH="/usr/local /usr"     # you might want to change this
  SEARCH_FOR="/include/redis.h"  # you most likely want to change this
  if test -r $PHP_REDIS/$SEARCH_FOR; then # path given as parameter
       REDIS_DIR=$PHP_REDIS
  else # search default path list
       AC_MSG_CHECKING([for redis files in default path])
  for i in $SEARCH_PATH ; do
       if test -r $i/$SEARCH_FOR; then
         REDIS_DIR=$i
         AC_MSG_RESULT(found in $i)
       fi
     done
  fi
  
  if test -z "$REDIS_DIR"; then
     AC_MSG_RESULT([not found])
     AC_MSG_ERROR([Please reinstall the redis distribution])
  fi

  dnl # --with-redis -> add include path
  PHP_ADD_INCLUDE($REDIS_DIR/include)

  dnl # --with-redis -> check for lib and symbol presence
  LIBNAME=redis # you may want to change this
  LIBSYMBOL=Module_init # you most likely want to change this 

  PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  [
     PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $REDIS_DIR/lib, REDIS_SHARED_LIBADD)
     AC_DEFINE(HAVE_REDISLIB,1,[ ])
   ],[
     AC_MSG_ERROR([wrong redis lib version or lib not found])
   ],[
     -L$REDIS_DIR/lib -lm -ldl
   ])
  
  PHP_SUBST(REDIS_SHARED_LIBADD)

  PHP_NEW_EXTENSION(redis, php_redis.c, $ext_shared)
fi
-
