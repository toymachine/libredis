/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

#ifndef __COMMON_H
#define __COMMON_H

#define DEFAULT_WRITE_BUFF_SIZE (1024 * 4)
#define DEFAULT_READ_BUFF_SIZE (1024 * 12)
#define DEFAULT_COMMAND_BUFF_SIZE 64
#define MAX_BUFF_SIZE (1024 * 1024 * 4)
#define MAX_CONNECTIONS 1024

#define ADDR_SIZE 255
#define SERV_SIZE 20

#ifndef MIN
#define MIN(a,b) ((a)>(b)?(b):(a))
#endif

#define XSTR(s) STR(s)
#define STR(s) #s

#define DEFAULT_IP_PORT 6379

#ifndef NDEBUG
#define DEBUG(args) (printf("DEBUG: "), printf args)
#else
#define DEBUG(args)
#endif

#define CR '\r'
#define LF '\n'
#define CRLF '\r\n'

#ifdef SINGLETHREADED
#define THREADLOCAL
#else
#define THREADLOCAL __thread
#endif

typedef char Byte;

//#include "alloc.h"

typedef struct _Command Command;
typedef struct _Reply Reply;

#endif
