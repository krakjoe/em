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
#include <spawn.h>

int posix_spawnp(pid_t *pid, const char *file,
                const posix_spawn_file_actions_t *actions,
                const posix_spawnattr_t *attributes,
                char *const argv[], char *const env[]) {
    (void) pid;
    (void) file;
    (void) actions;
    (void) attributes;
    (void) argv;
    (void) env;

    return -1; // not supported
}