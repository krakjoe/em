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

typedef struct _em_http_request_t {
    char* method;
    char* url;
    struct {
        char** keys;
        char** values;
        size_t length;
    } headers;
    char*  body;
    size_t length;
    size_t timeout;
} em_http_request_t;

em_http_request_t em_http_request_create(
    const char* url, php_stream_context* context);
ssize_t em_http_request(const char*  method,
    const char*  url,
    const char** hkeys,
    const char** hvalues,
    size_t       hlength,
    const char*  body,
    size_t       blength,
    size_t       timeout,
    uintptr_t abstract);
void em_http_request_destroy(em_http_request_t* request);
#endif