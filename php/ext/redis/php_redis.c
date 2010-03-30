#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "php.h"
#include "zend.h"
#include "php_redis.h"

#include "redis.h"

#define T_fromObj(T, ce, obj) (T *)Z_LVAL_P(zend_read_property(ce, obj, "handle", 6, 0))
#define T_getThis(T, ce) (T *)Z_LVAL_P(zend_read_property(ce, getThis(), "handle", 6, 0))
#define T_setThis(p, ce) zend_update_property_long(ce, getThis(), "handle", 6, (long)p);

zend_class_entry *batch_ce;
zend_class_entry *ketama_ce;
zend_class_entry *connection_ce;

/**************** KETAMA ***********************/


#define Ketama_getThis() T_getThis(Ketama, ketama_ce)
#define Ketama_setThis(p) T_setThis(p, ketama_ce)

PHP_METHOD(Ketama, __construct)
{
	Ketama_setThis(Ketama_new());
}

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
    PHP_ME(Ketama,  __construct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Ketama,  __destruct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    PHP_ME(Ketama,  add_server,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Ketama,  get_server,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Ketama,  get_server_addr,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Ketama,  create_continuum,  NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

/**************** CONNECTIONS ***********************/

#define Connection_getThis() T_getThis(Connection, connection_ce)
#define Connection_setThis(p) T_setThis(p, connection_ce)

PHP_METHOD(Connection, __construct)
{
	char *addr;
	int addr_len;

	if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "s", &addr, &addr_len) == FAILURE) {
		RETURN_NULL();
	}

	Connection_setThis(Connection_new(addr));
}

PHP_METHOD(Connection, __destruct)
{
	Connection_free(Connection_getThis());
	Connection_setThis(0);
}

PHP_METHOD(Connection, execute)
{
	zval *obj;

	zend_bool dispatch = 0;

	if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "O|b", &obj, batch_ce, &dispatch) == FAILURE) {
		RETURN_NULL();
	}

	Batch *batch = T_fromObj(Batch, batch_ce, obj);

	Connection_execute(Connection_getThis(), batch);

	if(dispatch) {
		Module_dispatch();
	}
}

function_entry connection_methods[] = {
    PHP_ME(Connection,  __construct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Connection,  __destruct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    PHP_ME(Connection,  execute,           NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

/**************** BATCH ***********************/


#define Batch_getThis() T_getThis(Batch, batch_ce)
#define Batch_setThis(p) T_setThis(p, batch_ce)

PHP_METHOD(Batch, __construct)
{
	Batch_setThis(Batch_new());

	char *str = NULL;
	int str_len = 0;
	long num_commands = 0;

	if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "|sl", &str, &str_len, &num_commands) == FAILURE) {
		RETURN_NULL();
	}

	if(str != NULL && str_len > 0) {
		Batch_write(Batch_getThis(), str, str_len, num_commands);
	}

}

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

	zval_dtor(reply_value);

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

function_entry batch_methods[] = {
    PHP_ME(Batch,  __construct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Batch,  __destruct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    PHP_ME(Batch,  write,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Batch,  next_reply,           NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

/***************** PHP MODULE **************************/

PHP_FUNCTION(Redis_dispatch)
{
	Module_dispatch();
}

PHP_MINIT_FUNCTION(redis)
{
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, "Redis_Ketama", ketama_methods);
    ketama_ce = zend_register_internal_class(&ce TSRMLS_CC);
    zend_declare_property_long(ketama_ce, "handle", 6, 0, ZEND_ACC_PRIVATE);

    INIT_CLASS_ENTRY(ce, "Redis_Connection", connection_methods);
    connection_ce = zend_register_internal_class(&ce TSRMLS_CC);
    zend_declare_property_long(connection_ce, "handle", 6, 0, ZEND_ACC_PRIVATE);

    INIT_CLASS_ENTRY(ce, "Redis_Batch", batch_methods);
    batch_ce = zend_register_internal_class(&ce TSRMLS_CC);
    zend_declare_property_long(batch_ce, "handle", 6, 0, ZEND_ACC_PRIVATE);

    Module_init();

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(redis)
{
	Module_free();

	return SUCCESS;
}

static function_entry redis_functions[] = {
    PHP_FE(Redis_dispatch, NULL)
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
    NULL,
    NULL,
    NULL,
#if ZEND_MODULE_API_NO >= 20010901
    PHP_REDIS_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_REDIS
ZEND_GET_MODULE(redis)
#endif

