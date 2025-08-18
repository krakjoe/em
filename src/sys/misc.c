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

#include <emscripten.h>
#include <unistd.h>

/**
 * the asan runtime links it's own reallocarray
 */
#if !defined(EM_SANITIZE_ASAN) && !defined(EM_SANITIZE_UBSAN)
void *reallocarray(void *ptr, size_t nmemb, size_t size) {
    if (nmemb && size && SIZE_MAX / nmemb < size) {
        return NULL;
    }
    return realloc(ptr, nmemb * size);
}
#endif