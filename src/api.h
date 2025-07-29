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

#ifndef HAVE_EM_API
#define HAVE_EM_API
#include <emscripten.h>

#include <php.h>

int EMSCRIPTEN_KEEPALIVE em_startup(void);
uintptr_t EMSCRIPTEN_KEEPALIVE em_run_string(
    const char* code, size_t length);
size_t EMSCRIPTEN_KEEPALIVE em_run_length(void);
void EMSCRIPTEN_KEEPALIVE em_run_free(void);
void EMSCRIPTEN_KEEPALIVE em_shutdown(void);
#endif