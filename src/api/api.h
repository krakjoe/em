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
void EMSCRIPTEN_KEEPALIVE em_shutdown(void);

bool EMSCRIPTEN_KEEPALIVE em_env_import(
  const char* env, size_t length);

/*
* em_run_request/string/script API shall return a uintptr_t to the context
*/

typedef void (*em_run_reaper_t)(uintptr_t context);

void EMSCRIPTEN_KEEPALIVE em_run_request(
    const char* env,  size_t elen,
    const char* head, size_t hlen,
    const char* body, size_t blen,
    em_run_reaper_t reaper);
void EMSCRIPTEN_KEEPALIVE em_run_string(
    const char* code, size_t length,
    em_run_reaper_t reaper);
void EMSCRIPTEN_KEEPALIVE em_run_script(
    const char* script,
    em_run_reaper_t reaper);

/**
 * The caller of em_run_request/string/script must use the address to access
 * the result and its length, they are also responsible for freeing the context
 */
char* EMSCRIPTEN_KEEPALIVE em_run_result(uintptr_t address);
size_t EMSCRIPTEN_KEEPALIVE em_run_length(uintptr_t address);
void EMSCRIPTEN_KEEPALIVE em_run_free(uintptr_t address);

zend_op_array* em_compile_script(
  const char* script);
zend_op_array* em_compile_string(
  const char* code, size_t length);
void em_execute(zend_op_array* ops);
#endif