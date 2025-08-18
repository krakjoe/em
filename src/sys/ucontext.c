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
#include <ucontext.h>

int getcontext(ucontext_t *ucp) {
    (void) ucp;

    return -1; // not supported, but fibers should handle this gracefully
}

void makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...) {
    (void) ucp;
    (void) func;
    (void) argc;

    // stub - fibers will fall back to alternative implementation
}

int swapcontext(ucontext_t *oucp, const ucontext_t *ucp) {
    (void) oucp;
    (void) ucp;

    return -1; // not supported
}