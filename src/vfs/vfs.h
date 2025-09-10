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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include <php.h>

#include <vfs/node.h>
#include <vfs/path.h>

extern php_stream_wrapper em_vfs_wrapper;

typedef struct _em_vfs_abstract_t {
    char*               data;
    ssize_t             length;
    size_t              position;
    size_t              maximum;
    em_vfs_node_t*      node;
} em_vfs_abstract_t;

void em_vfs_startup(void);
void em_vfs_activate(void);
void em_vfs_activate_ex(bool masquerading);
void em_vfs_deactivate(void);
void em_vfs_deactivate_ex(bool masquerading);
void em_vfs_masquerade(bool enabled);
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

em_vfs_node_t* em_vfs_get_node(const char* path);

zend_result em_vfs_stat_path(em_vfs_path_t* vpath, php_stream_statbuf *ssb, bool link);
em_vfs_node_t* em_vfs_resolve(em_vfs_path_t* vpath, bool make);

em_vfs_abstract_t* em_vfs_open(const char* path, const char* mode);
ssize_t em_vfs_read_offset(em_vfs_abstract_t* abstract, char* buffer, size_t count, size_t offset);
ssize_t em_vfs_write_offset(em_vfs_abstract_t* abstract, const char* buffer, size_t count, size_t offset);
static inline ssize_t em_vfs_read(em_vfs_abstract_t* abstract, char* buffer, size_t count) {
    return em_vfs_read_offset(abstract, buffer, count, abstract->position);
}
static inline ssize_t em_vfs_write(em_vfs_abstract_t* abstract, const char* buffer, size_t count) {
    return em_vfs_write_offset(abstract, buffer, count, abstract->position);
}
zend_result em_vfs_seek(em_vfs_abstract_t* abstract, zend_off_t offset, int whence, zend_off_t *position);
ssize_t em_vfs_truncate(em_vfs_abstract_t* abstract, size_t count);
ssize_t em_vfs_flush(em_vfs_abstract_t* abstract);
void em_vfs_close(em_vfs_abstract_t* abstract, bool sync);
void em_vfs_release(em_vfs_abstract_t* abstract);
#endif