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
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_libredis.h"

#include "redis.h"

#define DEFAULT_TIMEOUT_MS 5000

#define T_fromObj(T, ce, obj) (T *)Z_LVAL_P(zend_read_property(ce, obj, "handle", 6, 0))
#define T_getThis(T, ce) (T *)Z_LVAL_P(zend_read_property(ce, getThis(), "handle", 6, 0))
#define T_setThis(p, ce) zend_update_property_long(ce, getThis(), "handle", 6, (long)p);

zend_class_entry *batch_ce;
zend_class_entry *ketama_ce;
zend_class_entry *connection_ce;
zend_class_entry *redis_ce;
zend_class_entry *executor_ce;

//TODO proper php module globals?:
/* If you declare any globals in php_libredis.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(libredis)
*/

#define MAX_ERROR_SIZE 255

Module *g_module;
char g_module_error[MAX_ERROR_SIZE];

HashTable g_connections; //persistent connections
HashTable g_batch; //for keeping track of batch instances
HashTable g_ketama; //for keeping track of ketama instances
HashTable g_executor; //for keeping track of executor instances

//some forward decls
int Connection_execute_simple(Connection *connection, Batch *batch, long timeout);
void set_last_error_from_global_error();
void set_last_error_from_batch_error(Batch *batch);

/**************** KETAMA ***********************/


#define Ketama_getThis() T_getThis(Ketama, ketama_ce)
#define Ketama_setThis(p) T_setThis(p, ketama_ce)

PHP_METHOD(Ketama, __destruct)
{
    Ketama *ketama = Ketama_getThis();
    zend_hash_del(&g_ketama, (char *)&ketama, sizeof(ketama));
    Ketama_free(ketama);
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

PHP_METHOD(Ketama, get_server_ordinal)
{
    char *key;
    int key_len;

    if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
        RETURN_NULL();
    }

    RETURN_LONG(Ketama_get_server_ordinal(Ketama_getThis(), key, key_len));
}

PHP_METHOD(Ketama, get_server_address)
{
    long ordinal;

    if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "l", &ordinal) == FAILURE) {
        RETURN_NULL();
    }

    RETURN_STRING(Ketama_get_server_address(Ketama_getThis(), ordinal), 1);
}

PHP_METHOD(Ketama, create_continuum)
{
    Ketama_create_continuum(Ketama_getThis());
}

function_entry ketama_methods[] = {
    PHP_ME(Ketama,  __destruct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    PHP_ME(Ketama,  add_server,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Ketama,  get_server_ordinal,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Ketama,  get_server_address,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Ketama,  create_continuum,  NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

/**************** EXECUTOR ***********************/


#define Executor_getThis() T_getThis(Executor, executor_ce)
#define Executor_setThis(p) T_setThis(p, executor_ce)

PHP_METHOD(Executor, __destruct)
{
    Executor *executor = Executor_getThis();
    zend_hash_del(&g_executor, (char *)&executor, sizeof(executor));
    Executor_free(executor);
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

    if(-1 == Executor_add(Executor_getThis(), connection, batch)) {
       zend_error(E_ERROR, "%s", Module_last_error(g_module));
       RETURN_NULL();
    }

    RETURN_BOOL(1);
}

PHP_METHOD(Executor, execute)
{
    long timeout = DEFAULT_TIMEOUT_MS;

    if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "|l", &timeout) == FAILURE) {
        RETURN_NULL();
    }

    int execute_res = Executor_execute(Executor_getThis(), timeout);
    if(execute_res > 0) {
        RETURN_BOOL(1);
    }
    else if(execute_res == 0) {
        set_last_error_from_global_error();
        RETURN_BOOL(0);
    }
    else {
        zend_error(E_ERROR, "%s", Module_last_error(g_module));
        RETURN_NULL();
    }
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

int Connection_execute_simple(Connection *connection, Batch *batch, long timeout)
{
    Executor *executor = Executor_new();
    Executor_add(executor, connection, batch);
    int execute_result = Executor_execute(executor, timeout);
    Executor_free(executor);
    if(execute_result < 0) {
        zend_error(E_ERROR, "%s", Module_last_error(g_module));
    }
    return execute_result;
}

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
    RETURN_BOOL(Connection_execute_simple(Connection_getThis(), batch, timeout));
}

PHP_METHOD(Connection, set)
{
    char *key;
    int key_len;
    char *value;
    int value_len;
    long timeout = DEFAULT_TIMEOUT_MS;

    if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "ss|l", &key, &key_len, &value, &value_len, &timeout) == FAILURE) {
        RETURN_NULL();
    }

    Batch *batch = Batch_new();
    Batch_write_set(batch, key, key_len, value, value_len);
    if(Connection_execute_simple(Connection_getThis(), batch, timeout)) {
        ReplyType c_reply_type;
        char *c_reply_value;
        size_t c_reply_length;
        int level = Batch_next_reply(batch, &c_reply_type, &c_reply_value, &c_reply_length);
        if(level != 1) {
            zend_error(E_ERROR, "Unexpected level");
            RETVAL_NULL();
        }
        else {
            if(c_reply_type == RT_OK) {
                RETVAL_BOOL(1);
            }
            else if(c_reply_type == RT_ERROR) {
                set_last_error_from_batch_error(batch);
                RETVAL_BOOL(0);
            }
            else {
                zend_error(E_ERROR, "Unexpected reply type");
                RETVAL_NULL();
            }
        }
    }
    else {
        set_last_error_from_global_error();
        RETVAL_BOOL(0);
    }
    Batch_free(batch);
}

PHP_METHOD(Connection, get)
{
    char *key;
    int key_len;
    long timeout = DEFAULT_TIMEOUT_MS;

    if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &key, &key_len, &timeout) == FAILURE) {
        RETURN_BOOL(0);
    }

    Batch *batch = Batch_new();
    Batch_write_get(batch, key, key_len);

    if(Connection_execute_simple(Connection_getThis(), batch, timeout)) {

        ReplyType c_reply_type;
        char *c_reply_value;
        size_t c_reply_length;
        int level = Batch_next_reply(batch, &c_reply_type, &c_reply_value, &c_reply_length);
        if(level != 1) {
            zend_error(E_ERROR, "Unexpected level");
            RETVAL_NULL();
        }
        else {
            if(c_reply_type == RT_BULK) {
                if(c_reply_value != NULL && c_reply_length > 0) {
                    RETVAL_STRINGL(c_reply_value, c_reply_length, 1);
                }
                else {
                    RETVAL_EMPTY_STRING();
                }
            }
            else if(c_reply_type == RT_BULK_NIL) {
                RETVAL_NULL();
            }
            else if(c_reply_type == RT_ERROR) {
                set_last_error_from_batch_error(batch);
                RETVAL_BOOL(0);
            }
            else {
                zend_error(E_ERROR, "Unexpected reply type");
                RETVAL_NULL();
            }
        }
    }
    else {
        set_last_error_from_global_error();
        RETVAL_BOOL(0);
    }

    Batch_free(batch);
}


function_entry connection_methods[] = {
    PHP_ME(Connection,  __destruct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    PHP_ME(Connection,  execute,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Connection,  set,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Connection,  get,           NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

/**************** BATCH ***********************/


#define Batch_getThis() T_getThis(Batch, batch_ce)
#define Batch_setThis(p) T_setThis(p, batch_ce)

PHP_METHOD(Batch, __destruct)
{
    Batch *batch = Batch_getThis();
    zend_hash_del(&g_batch, (char *)&batch, sizeof(batch));
    Batch_free(batch);
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

    Batch_write_set(Batch_getThis(), key, key_len, value, value_len);

    RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(Batch, get)
{
    char *key;
    int key_len;

    if (zend_parse_parameters_ex(0, ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
        RETURN_NULL();
    }

    Batch_write_get(Batch_getThis(), key, key_len);

    RETURN_ZVAL(getThis(), 1, 0);
}

void Batch_cmd(Batch *batch, int num_args, zval ***varargs)
{
    for (int i = 0; i < num_args; i++) {
        // do something with varargs[i]
        if(Z_TYPE_PP(varargs[i]) != IS_STRING) {
            zend_error(E_ERROR, "all arguments must be strings!");
            return;
        }
    }
    // all ok, write the multibulk command
    Batch_write(batch, "*", 1, 1);
    Batch_write_decimal(batch, num_args);
    Batch_write(batch, "\r\n", 2, 0);
    for (int i = 0; i < num_args; i++) {
        Batch_write(batch, "$", 1, 0);
        Batch_write_decimal(batch, Z_STRLEN_PP(varargs[i]));
        Batch_write(batch, "\r\n", 2, 0);
        Batch_write(batch, Z_STRVAL_PP(varargs[i]), Z_STRLEN_PP(varargs[i]), 0);
        Batch_write(batch, "\r\n", 2, 0);
    }
}

PHP_METHOD(Batch, cmd)
{
    zval ***varargs = NULL;

    varargs = safe_emalloc(ZEND_NUM_ARGS(), sizeof(zval*), 0);

    if(zend_get_parameters_array_ex(ZEND_NUM_ARGS(), varargs) != FAILURE) {
        Batch_cmd(Batch_getThis(), ZEND_NUM_ARGS(), varargs);
    }

    if (varargs) {
        efree(varargs);
    }
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
       zend_error(E_ERROR, "Parameter wasn't passed by reference (reply_type)");
       RETURN_NULL();
    }
    if (!PZVAL_IS_REF(reply_value))
    {
        zend_error(E_ERROR, "Parameter wasn't passed by reference (reply_value)");
        RETURN_NULL();
    }
    if (!PZVAL_IS_REF(reply_length))
    {
        zend_error(E_ERROR, "Parameter wasn't passed by reference (reply_length)");
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

    RETURN_BOOL(Connection_execute_simple(connection, Batch_getThis(), timeout));
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_batch_next_rely, 0, 0, 3)
    ZEND_ARG_INFO(1, reply_type)
    ZEND_ARG_INFO(1, reply_value)
    ZEND_ARG_INFO(1, reply_length)
ZEND_END_ARG_INFO()

function_entry batch_methods[] = {
    PHP_ME(Batch,  __destruct,     NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    PHP_ME(Batch,  write,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Batch,  set,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Batch,  get,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Batch,  cmd,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Batch,  execute,           NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Batch,  next_reply,           arginfo_batch_next_rely, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

/***************** PHP MODULE **************************/

void set_last_error_from_global_error()
{
    strncpy(g_module_error, Module_last_error(g_module), MAX_ERROR_SIZE);
}

void set_last_error_from_batch_error(Batch *batch)
{
    char *batch_error = Batch_error(batch);
    if(batch_error != NULL) {
        strncpy(g_module_error, batch_error, MAX_ERROR_SIZE);
    }
    else {
        g_module_error[0] = '\0';
    }
}

PHP_FUNCTION(Libredis)
{
    object_init_ex(return_value, redis_ce);
}

PHP_METHOD(Redis, create_ketama)
{
    object_init_ex(return_value, ketama_ce);
    Ketama *ketama = Ketama_new();
    zend_update_property_long(ketama_ce, return_value, "handle", 6, (long)ketama);
    void *pDest;
    zend_hash_update(&g_ketama, (char *)&ketama, sizeof(ketama), &ketama, sizeof(ketama), &pDest);
}

PHP_METHOD(Redis, create_executor)
{
    object_init_ex(return_value, executor_ce);
    Executor *executor = Executor_new();
    zend_update_property_long(executor_ce, return_value, "handle", 6, (long)executor);
    void *pDest;
    zend_hash_update(&g_executor, (char *)&executor, sizeof(executor), &executor, sizeof(executor), &pDest);
}

PHP_METHOD(Redis, last_error)
{
    RETVAL_STRING(g_module_error, 1);
    g_module_error[0] = '\0';
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
        connection = Connection_new(addr);
        if(connection == NULL) {
             zend_error(E_ERROR, "%s", Module_last_error(g_module));
             RETURN_NULL();
        }
        else {
            zend_hash_update(&g_connections, addr, addr_len, &connection, sizeof(connection), &pDest);
        }
    }
    else {
        connection = *((Connection **)pDest);
    }

    object_init_ex(return_value, connection_ce);
    zend_update_property_long(connection_ce, return_value, "handle", 6, (long)connection);
}

PHP_METHOD(Redis, create_batch)
{
    Batch *batch = Batch_new();
    object_init_ex(return_value, batch_ce);
    zend_update_property_long(batch_ce, return_value, "handle", 6, (long)batch);
    void *pDest;
    zend_hash_update(&g_batch, (char *)&batch, sizeof(batch), &batch, sizeof(batch), &pDest);

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
    PHP_ME(Redis,  last_error,           NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};


/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(libredis)
{
    /* If you have INI entries, uncomment these lines
    REGISTER_INI_ENTRIES();
    */
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
    zend_hash_init(&g_batch, 128, NULL, NULL, 1);
    zend_hash_init(&g_ketama, 32, NULL, NULL, 1);
    zend_hash_init(&g_executor, 16, NULL, NULL, 1);

    g_module = Module_new();
    Module_set_alloc_alloc(g_module, __zend_malloc);
    Module_set_alloc_realloc(g_module, __zend_realloc);
    Module_set_alloc_free(g_module, free);
    Module_init(g_module);

    syslog(LOG_DEBUG, "libredis module init");
    closelog();

    return SUCCESS;
}
/* }}} */

#define SHUTDOWN_FREE_T(T) \
int shutdown_hash_free_ ## T(void *pDest TSRMLS_DC) \
{ \
    T *obj = *((T **)pDest); \
    T ## _free(obj); \
    return ZEND_HASH_APPLY_REMOVE; \
} \


SHUTDOWN_FREE_T(Batch)
SHUTDOWN_FREE_T(Ketama)
SHUTDOWN_FREE_T(Executor)
SHUTDOWN_FREE_T(Connection)

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(libredis)
{
    /* uncomment this line if you have INI entries
    UNREGISTER_INI_ENTRIES();
    */
    //free the persistent connections
    zend_hash_apply(&g_connections, shutdown_hash_free_Connection);
    zend_hash_destroy(&g_connections);
    zend_hash_destroy(&g_batch);
    zend_hash_destroy(&g_ketama);
    zend_hash_destroy(&g_executor);
    //todo check if the above really release all resource, what about the items in the hashtable?, are they released
    //hash destroy?

    Module_free(g_module);

    openlog("libredis", 0, LOG_LOCAL2);
    syslog(LOG_DEBUG, "libredis module shutdown complete, final alloc: %d\n", Module_get_allocated(g_module));
    closelog();

    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(libredis)
{
    openlog("libredis", 0, LOG_LOCAL2);
    syslog(LOG_DEBUG, "libredis request init");
    closelog();

    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(libredis)
{
    zend_hash_apply(&g_batch, shutdown_hash_free_Batch); //frees any batches still left, e.g. not freed by normal dispose
    zend_hash_apply(&g_ketama, shutdown_hash_free_Ketama); //frees any ketama still left, e.g. not freed by normal dispose
    zend_hash_apply(&g_executor, shutdown_hash_free_Executor); //frees any executor still left, e.g. not freed by normal dispose

    openlog("libredis", 0, LOG_LOCAL2);
    syslog(LOG_DEBUG, "libredis request shutdown complete, final alloc: %d, g_batchsz: %d\n", Module_get_allocated(g_module), g_batch.nNumOfElements);
    closelog();

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(libredis)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "libredis support", "enabled");
    php_info_print_table_end();

    /* Remove comments if you have entries in php.ini
    DISPLAY_INI_ENTRIES();
    */
}
/* }}} */

/* {{{ libredis_functions[]
 *
 * Every user visible function must have an entry in libredis_functions[].
 */
zend_function_entry libredis_functions[] = {
    PHP_FE(Libredis, NULL)
    {NULL, NULL, NULL}	/* Must be the last line in libredis_functions[] */
};
/* }}} */

/* {{{ libredis_module_entry
 */
zend_module_entry libredis_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    "libredis",
    libredis_functions,
    PHP_MINIT(libredis),
    PHP_MSHUTDOWN(libredis),
    PHP_RINIT(libredis),
    PHP_RSHUTDOWN(libredis),
    PHP_MINFO(libredis),
#if ZEND_MODULE_API_NO >= 20010901
    "0.1", /* Replace with version number for your extension */
#endif
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_LIBREDIS
ZEND_GET_MODULE(libredis)
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("libredis.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_libredis_globals, libredis_globals)
    STD_PHP_INI_ENTRY("libredis.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_libredis_globals, libredis_globals)
PHP_INI_END()
*/
/* }}} */

/* {{{ php_libredis_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_libredis_init_globals(zend_libredis_globals *libredis_globals)
{
    libredis_globals->global_value = 0;
    libredis_globals->global_string = NULL;
}
*/
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
