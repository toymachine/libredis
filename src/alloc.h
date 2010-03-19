#ifndef __ALLOC_H
#define __ALLOC_H

void *_Alloc_alloc(size_t size);
void _Alloc_free(void *obj, size_t size);

#define Alloc_alloc_T(T) (T *) _Alloc_alloc(sizeof(T))
#define Alloc_free_T(PT, T) _Alloc_free(PT, sizeof(T))

#define Alloc_alloc(SZ) _Alloc_alloc(SZ)
#define Alloc_free(PT, SZ) _Alloc_free(PT, SZ)

#define ALLOC_LIST_T(T, member) \
	static struct list_head T ## _free_list = LIST_HEAD_INIT(T ## _free_list); \
	\
	static inline int T ## _alloc(T **obj) \
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
	static inline int T ## _dealloc(T *obj) { \
		DEBUG(("list dealloc " #T "\n")); \
		list_add(&obj->member, &T ## _free_list); \
	} \

#endif

