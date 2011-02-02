/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

#ifndef __ALLOC_H
#define __ALLOC_H

#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "module.h"

static inline void *_Alloc_alloc(size_t size)
{
	GET_MODULE()->allocated += size;
	DEBUG(("alloc real: %d total now: %d\n", size, GET_MODULE()->allocated));
	return GET_MODULE()->alloc_malloc(size);
}

static inline void _Alloc_free(void *obj, size_t size)
{
	GET_MODULE()->alloc_free(obj);
	GET_MODULE()->allocated -= size;
	DEBUG(("dealloc real: %d total now: %d\n", size, GET_MODULE()->allocated));
}

static inline void *_Alloc_realloc(void *obj, size_t new_size, size_t old_size)
{
	GET_MODULE()->allocated -= old_size;
	GET_MODULE()->allocated += new_size;
	DEBUG(("realloc real: %d total now: %d\n", new_size, GET_MODULE()->allocated));
	return GET_MODULE()->alloc_realloc(obj, new_size);
}

#define Alloc_alloc_T(T) (T *) _Alloc_alloc(sizeof(T))
#define Alloc_free_T(PT, T) _Alloc_free(PT, sizeof(T))

#define Alloc_alloc(SZ) _Alloc_alloc(SZ)
#define Alloc_free(PT, SZ) _Alloc_free(PT, SZ)
#define Alloc_realloc(PT, SZ, OSZ) _Alloc_realloc(PT, SZ, OSZ)

#ifdef SINGLETHREADED
#define ALLOC_LIST_T(T, member) \
	static struct list_head T ## _free_list = LIST_HEAD_INIT(T ## _free_list); \
	\
	static inline int T ## _list_alloc(T **obj) \
	{ \
		if(list_empty(&T ## _free_list)) { \
			DEBUG(("real alloc " #T "\n")); \
			*obj = Alloc_alloc_T(T); \
			return 1;\
		} \
		else { \
			DEBUG(("list alloc " #T "\n")); \
			*obj = list_pop_T(T, member, &T ## _free_list); \
			return 0; \
		} \
	} \
	\
	static inline void T ## _list_free(T *obj, int final) { \
		if(final) { \
			DEBUG(("real free " #T "\n")); \
			Alloc_free_T(obj, T); \
		} \
		else { \
			DEBUG(("list free " #T "\n")); \
			list_add(&obj->member, &T ## _free_list); \
		} \
	} \
	\
	void _ ## T ## _free(T *obj, int final); \
	\
	void T ## _free(T *obj) \
	{ \
		_ ## T ## _free(obj, 0); \
	} \
	\
	void T ## _free_final() \
	{ \
		DEBUG((#T " free final\n")); \
		while(!list_empty(&T ## _free_list)) { \
			T *obj = list_pop_T(T, member, &T ## _free_list); \
			_ ## T ## _free(obj, 1); \
		} \
	}
#else
#define ALLOC_LIST_T(T, member) \
	static inline int T ## _list_alloc(T **obj) \
	{ \
		*obj = Alloc_alloc_T(T); \
		return 1; \
	} \
	\
	static inline void T ## _list_free(T *obj, int final) \
	{ \
		Alloc_free_T(obj, T); \
	} \
	\
	void _ ## T ## _free(T *obj, int final); \
	\
	void T ## _free(T *obj) \
	{ \
		_ ## T ## _free(obj, 1); \
	} \
	void T ## _free_final() { }
#endif

#endif

