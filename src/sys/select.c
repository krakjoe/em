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
#include <sys/types.h>
#include <inttypes.h>
#include <sys/select.h>

extern  int __syscall__newselect(
    int,
    intptr_t,
    intptr_t,
    intptr_t,
    intptr_t);

int select(
    int m,
    fd_set *r,
    fd_set* w,
    fd_set* e,
    struct timeval* t) {
    /**
     * We must yield control to javascript before any selects
     * this prevents unbounded hanging on select where a stream will only
     * become ready because javascript has made the request ...
     */
    emscripten_sleep(100);

    return __syscall__newselect(
        m,
        (intptr_t)r,
        (intptr_t)w,
        (intptr_t)e,
        (intptr_t)t);
}