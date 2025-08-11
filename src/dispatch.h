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

#ifndef HAVE_EM_DISPATCH
#define HAVE_EM_DISPATCH
#include <emscripten.h>

#include <SAPI.h>

#include "buffer.h"

extern HashTable __em_environ__;

typedef void(*em_dispatch_handler_t)(sapi_request_info* info);

typedef struct _em_dispatch_context_t {
    HashTable environ;
    struct {
        struct {
            em_buffer_t head;
            em_buffer_t body;
        } request;
        struct {
            em_buffer_t head;
            em_buffer_t body;
            em_buffer_t join;
        } response;
    } buffers;
    struct {
        zend_llist request;
        zend_llist response;
    } headers;
    sapi_request_info *info;
} em_dispatch_context_t;

typedef struct _em_dispatch_header_t {
    struct {
        char* data;
        size_t len;
    } key;
    struct {
        char* data;
        size_t len;
    } value;
} em_dispatch_header_t;

bool em_dispatch_env(HashTable* table, const char* env, size_t elen, bool persistence);
void em_dispatch_env_import(zval* vars);

typedef enum _em_dispatch_selector_t {
    EM_DISPATCH_HEAD,
    EM_DISPATCH_BODY,
} em_dispatch_selector_t;

void em_dispatch_header(const char* format, ...);

size_t em_dispatch_response(em_dispatch_selector_t selector, const char* buffer, size_t length);

static size_t em_dispatch_writer(const char* buffer, size_t length) {
    return em_dispatch_response(EM_DISPATCH_BODY, buffer, length);
}

em_dispatch_handler_t em_dispatch_setup(
    sapi_request_info* info,
    const char* env,  size_t elen,
    const char* head, size_t hlen,
    const char* body, size_t blen);
em_dispatch_handler_t em_dispatch_setup_script(
    sapi_request_info* info,
    const char* script);
em_dispatch_handler_t em_dispatch_setup_code(
    sapi_request_info* info,
    const char* code, size_t length);

void em_dispatch_cleanup(void);
void em_dispatch_startup(void);
void em_dispatch_shutdown(void);
#endif