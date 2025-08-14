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
#ifndef HAVE_EM_HTTP_REQUEST
#define HAVE_EM_HTTP_REQUEST

#include <emscripten.h>

extern HashTable __em_http_requests__;

typedef struct _em_http_request_t {
    uint32_t timeout;
    char* method;
    char* url;
    struct {
        char** keys;
        char** values;
        size_t length;
    } headers;
    char*    body;
    size_t   length;
} em_http_request_t;

void em_http_request_startup(void);
void em_http_request_shutdown(void);

em_http_request_t em_http_request_create(
    const char* url, php_stream_context* context);
int em_http_request_start(
    int id,
    uint32_t timeout,
    const char* method,
    const char* url,
    const char** hkeys, 
    const char** hvalues,
    size_t hlength,
    const char* body,
    size_t blength);
void em_http_request_destroy(em_http_request_t* request);

void EMSCRIPTEN_KEEPALIVE em_http_request_timeout(int id);
void EMSCRIPTEN_KEEPALIVE em_http_request_error(int id);
void EMSCRIPTEN_KEEPALIVE em_http_request_response(
    int id, int status, uintptr_t buffer, size_t length);

void em_http_request_event_set(em_http_abstract_t* abstract);
void em_http_request_event_clear(em_http_abstract_t* abstract);
#endif