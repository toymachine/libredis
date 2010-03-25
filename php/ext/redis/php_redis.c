#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_redis.h"

#include "redis.h"

zend_class_entry *ketama_ce;

#define Ketama_getThis() (Ketama *)Z_LVAL_P(zend_read_property(ketama_ce, getThis(), "handle", 6, 0))

PHP_METHOD(Ketama, __construct)
{
	Ketama *ketama = Ketama_new();
	zend_update_property_long(ketama_ce, getThis(), "handle", 6, (long)ketama);
}

PHP_METHOD(Ketama, __destruct)
{
	Ketama_free(Ketama_getThis());
	zend_update_property_long(ketama_ce, getThis(), "handle", 6, 0);
}

PHP_METHOD(Ketama, add_server)
{
	char *ip;
	int ip_len;
	long port;
	long weight;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sll", &ip, &ip_len, &port, &weight) == FAILURE) {
		RETURN_NULL();
	}

	Ketama_add_server(Ketama_getThis(), ip, port, weight);
}

PHP_METHOD(Ketama, get_server)
{
	char *key;
	int key_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
		RETURN_NULL();
	}

	RETURN_LONG(Ketama_get_server(Ketama_getThis(), key, key_len));
}

PHP_METHOD(Ketama, get_server_addr)
{
	long ordinal;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &ordinal) == FAILURE) {
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

PHP_MINIT_FUNCTION(redis)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "Ketama", ketama_methods);
    ketama_ce = zend_register_internal_class(&ce TSRMLS_CC);
    zend_declare_property_long(ketama_ce, "handle", 6, 0, ZEND_ACC_PRIVATE);

    Module_init();

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(redis)
{
	printf("shutdowns!!!\n");
	Module_free();

	return SUCCESS;
}

static function_entry redis_functions[] = {
    PHP_FE(hello_world, NULL)
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

PHP_FUNCTION(hello_world)
{
    RETURN_STRING("Hello World3!", 1);
}
