/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <syslog.h>

#include "php.h"
#include "zend.h"
#include "php_redis.h"

#include "redis.h"

#define DEFAULT_TIMEOUT_MS 3000

#define T_fromObj(T, ce, obj) (T *)Z_LVAL_P(zend_read_property(ce, obj, "handle", 6, 0))
#define T_getThis(T, ce) (T *)Z_LVAL_P(zend_read_property(ce, getThis(), "handle", 6, 0))
#define T_setThis(p, ce) zend_update_property_long(ce, getThis(), "handle", 6, (long)p);

zend_class_entry *batch_ce;
zend_class_entry *ketama_ce;
zend_class_entry *connection_ce;
zend_class_entry *redis_ce;
zend_class_entry *executor_ce;

//TODO proper php module globals?:
Module g_module;
HashTable g_connections;

/**************** KETAMA ***********************/


#define Ketama_getThis() T_getThis(Ketama, ketama_ce)
#define Ketama_setThis(p) T_setThis(p, ketama_ce)

PHP_METHOD(Ketama, __destruct)
{
	Ketama_free(Ketama_getThis());
	Ketama_setThis(0);
}

PHP_METHOD(Ketama, add_server)
{
	char *ip;
	int ip_len;
	long port;
	long weight;

	if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "sll", &ip, &ip_len, &port, &weight) == FAILURE) {
		RETURN_NULL();
	}

	Ketama_add_server(Ketama_getThis(), ip, port, weight);
}

PHP_METHOD(Ketama, get_server)
{
	char *key;
	int key_len;

	if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
		RETURN_NULL();
	}

	RETURN_LONG(Ketama_get_server(Ketama_getThis(), key, key_len));
}

PHP_METHOD(Ketama, get_server_addr)
{
	long ordinal;

	if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "l", &ordinal) == FAILURE) {
		RETURN_NULL();
	}

	RETURN_STRING(Ketama_get_server_addr(Ketama_getThis(), ordinal), 1);
}

PHP_METHOD(Ketama, create_continuum)
{
	Ketama_create_continuum(Ketama_getThis());
}

function_entry ketama_methods[] = {
    PHP_ME(Ketama,  __destruct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    PHP_ME(Ketama,  add_server,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Ketama,  get_server,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Ketama,  get_server_addr,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Ketama,  create_continuum,  NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

/**************** EXECUTOR ***********************/


#define Executor_getThis() T_getThis(Executor, executor_ce)
#define Executor_setThis(p) T_setThis(p, executor_ce)

PHP_METHOD(Executor, __destruct)
{
	Executor_free(Executor_getThis());
	Executor_setThis(0);
}

PHP_METHOD(Executor, add)
{
	zval *z_connection;
	zval *z_batch;

	if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "OO", &z_connection, connection_ce, &z_batch, batch_ce) == FAILURE) {
		RETURN_NULL();
	}

	Connection *connection = T_fromObj(Connection, connection_ce, z_connection);
	Batch *batch = T_fromObj(Batch, batch_ce, z_batch);

	Executor_add(Executor_getThis(), connection, batch);
}

PHP_METHOD(Executor, execute)
{
	long timeout = DEFAULT_TIMEOUT_MS;

	if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "|l", &timeout) == FAILURE) {
		RETURN_NULL();
	}

	Executor_execute(Executor_getThis(), timeout);
}

function_entry executor_methods[] = {
    PHP_ME(Executor,  __destruct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    PHP_ME(Executor,  add,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Executor,  execute,           NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};


/**************** CONNECTIONS ***********************/

#define Connection_getThis() T_getThis(Connection, connection_ce)
#define Connection_setThis(p) T_setThis(p, connection_ce)

PHP_METHOD(Connection, __destruct)
{
	//note that we not 'free' the real connection, because that is persistent
	Connection_setThis(0);
}

PHP_METHOD(Connection, execute)
{
	zval *z_batch;
	long timeout = DEFAULT_TIMEOUT_MS;

	if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "O|l", &z_batch, batch_ce, &timeout) == FAILURE) {
		RETURN_NULL();
	}

	Batch *batch = T_fromObj(Batch, batch_ce, z_batch);
	Executor *executor = Executor_new();
	Executor_add(executor, Connection_getThis(), batch);
	Executor_execute(executor, timeout);
	Executor_free(executor);
}


function_entry connection_methods[] = {
    PHP_ME(Connection,  __destruct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    PHP_ME(Connection,  execute,           NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

/**************** BATCH ***********************/


#define Batch_getThis() T_getThis(Batch, batch_ce)
#define Batch_setThis(p) T_setThis(p, batch_ce)


PHP_METHOD(Batch, __destruct)
{
	Batch_free(Batch_getThis());
	Batch_setThis(0);
}

PHP_METHOD(Batch, write)
{
	char *str;
	int str_len;
	long num_commands = 0;

	if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &str, &str_len, &num_commands) == FAILURE) {
		RETURN_NULL();
	}

	Batch_write(Batch_getThis(), str, str_len, num_commands);
}

PHP_METHOD(Batch, set)
{
	char *key;
	int key_len;
	char *value;
	int value_len;

	if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "ss", &key, &key_len, &value, &value_len) == FAILURE) {
		RETURN_NULL();
	}

	Batch *batch = Batch_getThis();
	Batch_write(batch, "SET ", 4, 0);
	Batch_write(batch, key, key_len, 0);
	char buff[16];
	int bufl = snprintf(buff, 16, " %d\r\n", value_len);
	Batch_write(batch, buff, bufl, 0);
	Batch_write(batch, value, value_len, 0);
	Batch_write(batch, "\r\n", 2, 1);

	RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(Batch, get)
{
	char *key;
	int key_len;

	if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
		RETURN_NULL();
	}

	Batch *batch = Batch_getThis();
	Batch_write(batch, "GET ", 4, 0);
	Batch_write(batch, key, key_len, 0);
	Batch_write(batch, "\r\n", 2, 1);

	RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(Batch, next_reply)
{
	zval *reply_type;
	zval *reply_value;
	zval *reply_length;

	//not using parameters_ex because of byref args
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz", &reply_type, &reply_value, &reply_length) == FAILURE) {
		RETURN_NULL();
	}

	if (!PZVAL_IS_REF(reply_type))
	{
	   zend_error(E_WARNING, "Parameter wasn't passed by reference (reply_type)");
	   RETURN_NULL();
	}
	if (!PZVAL_IS_REF(reply_value))
	{
	    zend_error(E_WARNING, "Parameter wasn't passed by reference (reply_value)");
	    RETURN_NULL();
	}
	if (!PZVAL_IS_REF(reply_length))
	{
	    zend_error(E_WARNING, "Parameter wasn't passed by reference (reply_length)");
	    RETURN_NULL();
	}

	ReplyType c_reply_type;
	char *c_reply_value;
	size_t c_reply_length;

	int res = Batch_next_reply(Batch_getThis(), &c_reply_type, &c_reply_value, &c_reply_length);

	ZVAL_LONG(reply_type, c_reply_type);

	zval_dtor(reply_value); //make sure any previous result is discarded (otherwise we would leak memory here)

    if(c_reply_type == RT_OK ||
       c_reply_type == RT_ERROR ||
       c_reply_type == RT_BULK) {
		if(c_reply_value != NULL && c_reply_length > 0) {
			ZVAL_STRINGL(reply_value, c_reply_value, c_reply_length, 1);
		}
		else {
			ZVAL_EMPTY_STRING(reply_value);
		}
    }
    else if(c_reply_type == RT_INTEGER) {
    	char *end_value = c_reply_value + c_reply_length;
    	ZVAL_LONG(reply_value, strtol(c_reply_value, &end_value, 10));
    }
    else {
    	ZVAL_NULL(reply_value);
    }

	ZVAL_LONG(reply_length, c_reply_length);

	RETURN_LONG(res);
}

PHP_METHOD(Batch, execute)
{
	zval *z_connection;
	long timeout = DEFAULT_TIMEOUT_MS;

	if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "O|l", &z_connection, connection_ce, &timeout) == FAILURE) {
		RETURN_NULL();
	}

	Connection *connection = T_fromObj(Connection, connection_ce, z_connection);
	Executor *executor = Executor_new();
	Executor_add(executor, connection, Batch_getThis());
	Executor_execute(executor, timeout);
	Executor_free(executor);
}

function_entry batch_methods[] = {
    PHP_ME(Batch,  __destruct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    PHP_ME(Batch,  write,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Batch,  set,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Batch,  get,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Batch,  execute,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Batch,  next_reply,           NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

/***************** PHP MODULE **************************/

PHP_FUNCTION(Libredis)
{
	object_init_ex(return_value, redis_ce);
}

PHP_METHOD(Redis, create_ketama)
{
	object_init_ex(return_value, ketama_ce);
	zend_update_property_long(ketama_ce, return_value, "handle", 6, (long)Ketama_new());
}

PHP_METHOD(Redis, create_executor)
{
	object_init_ex(return_value, executor_ce);
	zend_update_property_long(executor_ce, return_value, "handle", 6, (long)Executor_new());
}

PHP_METHOD(Redis, get_connection)
{
	char *addr;
	int addr_len;

	if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "s", &addr, &addr_len) == FAILURE) {
		RETURN_NULL();
	}

	Connection *connection;
	void *pDest;
	if(FAILURE == zend_hash_find(&g_connections, addr, addr_len, &pDest)) {
		//syslog(LOG_DEBUG, "connection not found: %s", addr);
		connection = Connection_new(addr);
		if(connection == NULL) {
			 zend_error(E_ERROR, "%s", g_module.error);
		}
		//syslog(LOG_DEBUG, "connection created addr: %p", connection);
		zend_hash_update(&g_connections, addr, addr_len, &connection, sizeof(connection), &pDest);
	}
	else {
		connection = *((Connection **)pDest);
		//syslog(LOG_DEBUG, "connection found: %s, %p", addr, connection);
	}

	object_init_ex(return_value, connection_ce);
	zend_update_property_long(connection_ce, return_value, "handle", 6, (long)connection);
}

int _shutdown_free_connection(void *pDest TSRMLS_DC)
{
	Connection *connection = *((Connection **)pDest);
	Connection_free(connection);
	//syslog(LOG_DEBUG, "connection freed: %p", connection);
	return ZEND_HASH_APPLY_KEEP;
}

PHP_METHOD(Redis, create_batch)
{
	Batch *batch = Batch_new();
	object_init_ex(return_value, batch_ce);
	zend_update_property_long(batch_ce, return_value, "handle", 6, (long)batch);

	char *str = NULL;
	int str_len = 0;
	long num_commands = 0;

	if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "|sl", &str, &str_len, &num_commands) == FAILURE) {
		RETURN_NULL();
	}

	if(str != NULL && str_len > 0) {
		Batch_write(batch, str, str_len, num_commands);
	}

}

function_entry redis_methods[] = {
    PHP_ME(Redis,  create_ketama,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Redis,  create_executor,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Redis,  create_batch,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Redis,  get_connection,           NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

PHP_MINIT_FUNCTION(redis)
{
    zend_class_entry ce;

    openlog("libredis", 0, LOG_LOCAL2);

    INIT_CLASS_ENTRY(ce, "_Libredis_Ketama", ketama_methods);
    ketama_ce = zend_register_internal_class(&ce TSRMLS_CC);
    zend_declare_property_long(ketama_ce, "handle", 6, 0, ZEND_ACC_PRIVATE);

    INIT_CLASS_ENTRY(ce, "_Libredis_Executor", executor_methods);
    executor_ce = zend_register_internal_class(&ce TSRMLS_CC);
    zend_declare_property_long(executor_ce, "handle", 6, 0, ZEND_ACC_PRIVATE);

    INIT_CLASS_ENTRY(ce, "_Libredis_Connection", connection_methods);
    connection_ce = zend_register_internal_class(&ce TSRMLS_CC);
    zend_declare_property_long(connection_ce, "handle", 6, 0, ZEND_ACC_PRIVATE);

    INIT_CLASS_ENTRY(ce, "_Libredis_Batch", batch_methods);
    batch_ce = zend_register_internal_class(&ce TSRMLS_CC);
    zend_declare_property_long(batch_ce, "handle", 6, 0, ZEND_ACC_PRIVATE);

    INIT_CLASS_ENTRY(ce, "_Libredis_Redis", redis_methods);
    redis_ce = zend_register_internal_class(&ce TSRMLS_CC);

    //init hashtable for persistent connections
    zend_hash_init(&g_connections, 128, NULL, NULL, 1);

    g_module.size = sizeof(Module);
    g_module.alloc_malloc = __zend_malloc;
    g_module.alloc_realloc = __zend_realloc;
    g_module.alloc_free = free;
    Module_init(&g_module);

    openlog("libredis", 0, LOG_LOCAL2);
    syslog(LOG_DEBUG, "libredis module init");

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(redis)
{
	//free the persistent connections
	zend_hash_apply(&g_connections, _shutdown_free_connection);
	zend_hash_destroy(&g_connections);
	//todo check if the above really release all resource, what about the items in the hashtable?, are they released
	//hash destroy?

	Module_free();

	openlog("libredis", 0, LOG_LOCAL2);
	syslog(LOG_DEBUG, "libredis module shutdown complete, final alloc: %d\n", g_module.allocated);

	return SUCCESS;
}

PHP_RINIT_FUNCTION(redis)
{
	openlog("libredis", 0, LOG_LOCAL2);
    syslog(LOG_DEBUG, "libredis request init");

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(redis)
{
	openlog("libredis", 0, LOG_LOCAL2);
	syslog(LOG_DEBUG, "libredis request shutdown complete, final alloc: %d\n", g_module.allocated);


	return SUCCESS;
}

static function_entry redis_functions[] = {
    PHP_FE(Libredis, NULL)
    {NULL, NULL, NULL}
};

zend_module_entry redis_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_REDIS_EXTNAME,
    redis_functions, /* functions */
    PHP_MINIT(redis), /* MINIT */
    PHP_MSHUTDOWN(redis), /* MSHUTDOWN */
    PHP_RINIT(redis), /* RINIT */
    PHP_RSHUTDOWN(redis), /* RSHUTDOWN */
    NULL,
#if ZEND_MODULE_API_NO >= 20010901
    PHP_REDIS_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_REDIS
ZEND_GET_MODULE(redis)
#endif

