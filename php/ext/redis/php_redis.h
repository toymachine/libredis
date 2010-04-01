/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

#ifndef PHP_REDIS_H
#define PHP_REDIS_H 1

#define PHP_REDIS_VERSION "1.0"
#define PHP_REDIS_EXTNAME "redis"

extern zend_module_entry redis_module_entry;
#define phpext_redis_ptr &redis_module_entry

#endif
