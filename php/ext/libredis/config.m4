dnl $Id$
dnl config.m4 for extension libredis

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

PHP_ARG_WITH(libredis, for libredis support,
[  --with-libredis             Include libredis support])

dnl Otherwise use enable:

dnl PHP_ARG_ENABLE(libredis, whether to enable libredis support,
dnl Make sure that the comment is aligned:
dnl [  --enable-libredis           Enable libredis support])

if test "$PHP_LIBREDIS" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-libredis -> check with-path
  SEARCH_PATH="/usr/local /usr"     # you might want to change this
  SEARCH_FOR="/libredis/redis.h"  # you most likely want to change this
  if test -r $PHP_LIBREDIS/$SEARCH_FOR; then # path given as parameter
     LIBREDIS_DIR=$PHP_LIBREDIS
  else # search default path list
     AC_MSG_CHECKING([for libredis files in default path])
    for i in $SEARCH_PATH ; do
       if test -r $i/$SEARCH_FOR; then
         LIBREDIS_DIR=$i
         AC_MSG_RESULT(found in $i)
       fi
     done
  fi
  
  if test -z "$LIBREDIS_DIR"; then
     AC_MSG_RESULT([not found])
     AC_MSG_ERROR([Please reinstall the libredis distribution])
  fi

  dnl # --with-libredis -> add include path
  PHP_ADD_INCLUDE($LIBREDIS_DIR/libredis)

  dnl # --with-libredis -> check for lib and symbol presence
  dnl LIBNAME=libredis # you may want to change this
  dnl LIBSYMBOL=libredis # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $LIBREDIS_DIR/lib, LIBREDIS_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_LIBREDISLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong libredis lib version or lib not found])
  dnl ],[
  dnl   -L$LIBREDIS_DIR/lib -lm -ldl
  dnl ])

  PHP_SUBST(LIBREDIS_SHARED_LIBADD)

  CFLAGS="-std=gnu99 $CFLAGS -pedantic -Wall -DNDEBUG"

  PHP_ADD_LIBRARY(rt,, LIBREDIS_SHARED_LIBADD)

  PHP_NEW_EXTENSION(libredis, libredis.c batch.c connection.c ketama.c md5.c module.c parser.c buffer.c, $ext_shared)
fi
