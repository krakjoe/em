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

#ifndef HAVE_EM_NODE
#define HAVE_EM_NODE

#include <php.h>

typedef struct _em_vfs_node_t em_vfs_node_t;

typedef enum _em_vfs_node_kind_t {
    EM_VFS_INV  = 0,
    EM_VFS_DIR  = 1,
    EM_VFS_FILE = 2,
} em_vfs_node_kind_t;

struct _em_vfs_node_t {
    uint32_t refcount;
    em_vfs_node_kind_t kind;
    char* name;

    union {
        struct {
            char* content;
            size_t size;
            time_t created;
            time_t modified;
        } file;

        struct {
            HashTable children;    // name -> em_vfs_node_t*
            time_t created;
        } dir;
    } data;

    em_vfs_node_t* parent;
};

zend_result em_vfs_node_stat(
    em_vfs_node_t* node, php_stream_statbuf *ssb, bool link);
em_vfs_node_t* em_vfs_node_mkfile(
    em_vfs_node_t* parent, const char* name);
em_vfs_node_t* em_vfs_node_mkdir(
    em_vfs_node_t* parent, const char* name);
static em_vfs_node_t*
    em_vfs_node_copy(
        em_vfs_node_t* node) {
    node->refcount++;
    return node;
}
char* em_vfs_node_path(em_vfs_node_t* node);
void em_vfs_node_release(em_vfs_node_t* node);
void em_vfs_node_dtor(zval *zv);
void em_vfs_node_free(em_vfs_node_t* node);
#endif