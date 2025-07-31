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

#ifndef HAVE_EM_VFS
#define HAVE_EM_VFS
#include <emscripten.h>

#include <php.h>

#include "node.h"
#include "path.h"

typedef struct _em_vfs_abstract_t {
    char*               data;
    ssize_t             length;
    size_t              position;
    size_t              maximum;
    em_vfs_node_t*      node;
} em_vfs_abstract_t;

void em_vfs_startup(void);
void em_vfs_activate(void);
void em_vfs_deactivate(void);
void em_vfs_shutdown(void);

bool EMSCRIPTEN_KEEPALIVE
    em_vfs_put(
        const char* path,
        const char* data, size_t length);
bool EMSCRIPTEN_KEEPALIVE
    em_vfs_unlink(
        const char* path,
        bool directories);
bool EMSCRIPTEN_KEEPALIVE
    em_vfs_mkdir(const char* path);
bool EMSCRIPTEN_KEEPALIVE
    em_vfs_move(
        const char* from,
        const char* to);
void* EMSCRIPTEN_KEEPALIVE
    em_vfs_get_address(const char* path);
ssize_t EMSCRIPTEN_KEEPALIVE
    em_vfs_get_length(const char* path);
void EMSCRIPTEN_KEEPALIVE em_vfs_reset(void);

em_vfs_node_t* em_vfs_resolve(em_vfs_path_t* vpath, bool make);
#endif