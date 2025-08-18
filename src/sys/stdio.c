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
#include <php.h>

/**
 * Maximum number of file descriptors allowed
 */
int getdtablesize(void) {
    return 1024; // reasonable default
}

/**
 * We map duplicated descriptors so that we can determine
 * if a descriptor is actually stdout/stderr during stream writing
 * at php layer
 */
int em_stdio_map[1024] = {0};
int em_stdio_dup[1024] = {0};

extern int __syscall_dup(int fd);
extern int __syscall_dup3(
    int old, int new, int flags);

int dup(int fd) {
    int duped = 
        __syscall_dup(fd);
    em_stdio_map[duped] = fd;
    em_stdio_dup[fd]    = duped;
    return duped;
}

int dup3(int old, int new, int flags);
int dup2(int old, int new) {
    if (em_stdio_dup[new]) {
        return em_stdio_dup[new];
    }
    return dup3(old, new, 0);
}

int dup3(int old, int new, int flags) {
    if (em_stdio_dup[new]) {
        return em_stdio_dup[new];
    }

    int duped =
        __syscall_dup3(old, new, flags);
    em_stdio_map[duped] = old;
    em_stdio_dup[old]   = duped;
    return duped;
}
