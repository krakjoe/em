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

#ifndef HAVE_EM_BUFFER
#define HAVE_EM_BUFFER

#include <stdbool.h>
#include <sys/types.h>

typedef struct _em_buffer_t {
    char* value;
    size_t length;
    size_t max;
    size_t position;
    char*  token;
} em_buffer_t;

#define EM_BUFFER_EMPTY \
  (em_buffer_t) {NULL, 0, 0, 0, NULL}

size_t em_buffer_write(em_buffer_t* buffer, const char* buf, size_t len);
void em_buffer_clear(em_buffer_t* buffer, bool _free);
size_t em_buffer_join(em_buffer_t* buffer, em_buffer_t* head, em_buffer_t* body);
#endif