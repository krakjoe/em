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

typedef void(*em_dispatch_handler_t)(sapi_request_info* info);

em_dispatch_handler_t em_dispatch_setup(
    sapi_request_info* info,
    const char* method, const char* uri, const char* mime,
    const char* request, size_t length);
void em_dispatch_cleanup(sapi_request_info* info);
#endif