/*
  +----------------------------------------------------------------------+
  | em                                                                   |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2025                                       |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
 */

#include <api/api.h>
#include <srv/dispatch.h>

HashTable __em_environ__;

void em_env_startup(void) {
    zend_hash_init(
        &__em_environ__, 8, NULL,
        ZVAL_PTR_DTOR, 1);
}

bool em_env_set(const char* key, const char* value, int overwrite) {
    em_dispatch_context_t* context =
        (em_dispatch_context_t*)
            SG(server_context);

    HashTable* table = (!context) ?
        &__em_environ__ : &context->environ;
    bool persistence = (table == &__em_environ__);

    zend_string* keyed =
        zend_string_init(
            key, strlen(key),
            persistence);
    zend_string* valued =
        zend_string_init(
            value, strlen(value),
            persistence);

    zval store;
    ZVAL_STR(
        &store, valued);
    zval* result;
    if (overwrite) {
        result =
            zend_hash_update(
                table,
                keyed,
                &store);
    } else {
        result =
            zend_hash_add(
                table,
                keyed,
                &store);
    }
    zend_string_release(keyed);

    return (result != NULL);
}

bool em_env_unset(const char* name) {
    em_dispatch_context_t* context =
        (em_dispatch_context_t*)
            SG(server_context);

    HashTable* table = (!context) ?
        &__em_environ__ :
        &context->environ;

    return zend_hash_str_del(
        table,
        name, strlen(name)) == SUCCESS;
}

char* em_env_get(const char* name) {
    em_dispatch_context_t* context =
        (em_dispatch_context_t*)
            SG(server_context);
    HashTable* table = context ?
        &context->environ :
        &__em_environ__;

    zval* item = zend_hash_str_find(
        table, name, strlen(name));

    if (!item) {
        return NULL;
    }

    return Z_STRVAL_P(item);
}

int setenv(const char* name, const char* value, int overwrite) {
    if (!name || strchr(name, '=')) {
        errno =
            EINVAL;
        return FAILURE;
    }

    if (!em_env_set(name, value, overwrite)) {
        errno =
            ENOMEM;
        return FAILURE;
    }

    return SUCCESS;
}

int unsetenv(const char* name) {
    if (!name || strchr(name, '=')) {
        errno =
            EINVAL;
        return FAILURE;
    }

    if (!em_env_unset(name)) {
        return FAILURE;
    }

    return SUCCESS;
}

char* getenv(const char* name) {
    if (!name || strchr(name, '=')) {
        errno =
            EINVAL;
        return NULL;
    }

    return em_env_get(name);
}

int putenv(char* string) {
    char* key   = strdup(string);
    char* split = strchr(key, '=');
    if (!split) {
        /* glibc extension */
        if (!em_env_unset(key)) {
            free(key);
            return FAILURE;
        }
        free(key);
        return SUCCESS;
    }

    char* value =
        strchr(key, '=');
    key[
        value - key] = 0;
    value++;

    if (!em_env_set(key, value, true)) {
        free(key);
        return FAILURE;
    }

    free(key);
    return SUCCESS;
}

void em_env_shutdown(void) {
    zend_hash_destroy(&__em_environ__);
}

