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

#ifndef HAVE_EM_HTTP
#define HAVE_EM_HTTP

#include <php.h>
#include <errno.h>
#include <emscripten.h>

typedef enum {
    EM_HTTP_WAITING =  1,
    EM_HTTP_COMPLETE = 2,
    EM_HTTP_ERROR =    4,
    EM_HTTP_TIMEOUT =  8
} em_http_state_t;

typedef struct _em_http_options_t {
    bool blocking;
} em_http_options_t;

typedef struct _em_http_abstract_t {
    uint32_t          refcount;
    uint32_t          id;
    em_http_state_t   state;
    em_http_options_t options;
    struct {
      bool drained;
      char drain;
      int  pipe[2];
    } event;
    struct {
        char*       data;
        size_t      length;
        size_t      position;
    } response;
} em_http_abstract_t;

void em_http_startup(void);
void em_http_activate(void);
void em_http_deactivate(void);
void em_http_shutdown(void);

void em_http_abstract_destroy(zval* zv);
void em_http_abstract_free(
  em_http_abstract_t* abstract);
#endif