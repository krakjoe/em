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

#ifndef HAVE_EM_ITERATOR
#define HAVE_EM_ITERATOR

#include <vfs/node.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

typedef struct _em_vfs_iterator_t {
    em_vfs_node_t* node;
    em_vfs_node_t* current;
    HashPosition   position;
} em_vfs_iterator_t;

void* EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator(const char* path);
ssize_t EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_count(void* iterator);
uintptr_t EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_path(void* iterator);

uintptr_t EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_name(void* iterator);
uint8_t EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_kind(void* iterator);
ssize_t EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_length(void* iterator);
uintptr_t EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_address(void* iterator);
time_t EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_created(void* iterator);
time_t EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_modified(void* iterator);

bool EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_reset(void* iterator);
bool EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_next(void* iterator);
void EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_free(void* iterator);
#endif