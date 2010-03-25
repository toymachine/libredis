#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_redis.h"

#include "redis.h"

zend_class_entry *ketama_ce;

PHP_METHOD(Ketama, __construct)
{
	printf("bla1");
	Ketama *ketama = Ketama_new();
	printf("bla2");
}

PHP_METHOD(Ketama, add_server)
{
	printf("add srv!");
}

function_entry ketama_methods[] = {
    PHP_ME(Ketama,  __construct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Ketama,  add_server,           NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

PHP_MINIT_FUNCTION(redis)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "Ketama", ketama_methods);
    ketama_ce = zend_register_internal_class(&ce TSRMLS_CC);
    zend_declare_property_null(ketama_ce, "handle", 6, ZEND_ACC_PRIVATE);
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
    NULL,
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
