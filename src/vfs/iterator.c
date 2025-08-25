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

#include "vfs.h"
#include "node.h"
#include "path.h"
#include "iterator.h"

typedef struct _em_vfs_iterator_t {
    em_vfs_node_t* node;
    em_vfs_node_t* current;
    HashPosition   position;
} em_vfs_iterator_t;

static zend_always_inline bool em_vfs_iterator_update(em_vfs_iterator_t* iterator) {
    zval* zv =
        zend_hash_get_current_data_ex(
            &iterator->node->data.dir.children,
            &iterator->position);
    if (!zv) {
        iterator->current = NULL;
        return false;
    }
    iterator->current = Z_PTR_P(zv);
    return true;
}

void* EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator(const char* path) {
    em_vfs_path_t* vpath =
        em_vfs_mkpath(path, true);
    em_vfs_node_t* node =
        em_vfs_resolve(vpath, false);
    em_vfs_path_release(vpath);

    if (!node) {
        return NULL;
    }

    if (node->kind != EM_VFS_DIR) {
        return NULL;
    }

    em_vfs_iterator_t* iterator = pecalloc(
        1, sizeof(em_vfs_iterator_t), 1);
    iterator->node = node;
    zend_hash_internal_pointer_reset_ex(
        &iterator->node->data.dir.children,
        &iterator->position);
    em_vfs_iterator_update(iterator);
    return iterator;
}

ssize_t EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_count(void* address) {
    em_vfs_iterator_t* iterator =
        (em_vfs_iterator_t*) address;
    return zend_hash_num_elements(
        &iterator->node->data.dir.children);
}

uint8_t EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_kind(void* address) {
    em_vfs_iterator_t* iterator =
        (em_vfs_iterator_t*) address;
    if (!iterator->current) {
        return EM_VFS_INV;
    }
    return iterator->current->kind;
}

uintptr_t EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_name(void* address) {
    em_vfs_iterator_t* iterator =
        (em_vfs_iterator_t*) address;
    if (!iterator->current ||
        !iterator->current->name) {
        return -1;
    }
    return (uintptr_t) iterator->current->name;
}

ssize_t EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_length(void* address) {
    em_vfs_iterator_t* iterator =
        (em_vfs_iterator_t*) address;
    if (!iterator->current) {
        return -1;
    }
    if (iterator->current->kind != EM_VFS_FILE) {
        return -1;
    }
    return iterator->current->data.file.size;
}

uintptr_t EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_address(void* address) {
    em_vfs_iterator_t* iterator =
        (em_vfs_iterator_t*) address;
    if (!iterator->current) {
        return -1;
    }
    if (iterator->current->kind != EM_VFS_FILE) {
        return -1;
    }
    return (uintptr_t)
        iterator->current->data.file.content;
}

time_t EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_created(void* address) {
    em_vfs_iterator_t* iterator =
        (em_vfs_iterator_t*) address;
    if (!iterator->current) {
        return -1;
    }
    if (iterator->current->kind == EM_VFS_FILE) {
        return iterator->current->data.file.created;
    }
    return iterator->current->data.dir.created;
}

time_t EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_modified(void* address) {
    em_vfs_iterator_t* iterator =
        (em_vfs_iterator_t*) address;
    if (!iterator->current) {
        return -1;
    }
    if (iterator->current->kind != EM_VFS_FILE) {
        return -1;
    }
    return iterator->current->data.file.modified;
}

bool EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_next(void* address) {
    em_vfs_iterator_t* iterator =
        (em_vfs_iterator_t*) address;
    if (zend_hash_move_forward_ex(
        &iterator->node->data.dir.children,
        &iterator->position) != SUCCESS) {
        return false;
    }

    return em_vfs_iterator_update(iterator);
}

bool EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_reset(void* address) {
    em_vfs_iterator_t* iterator =
        (em_vfs_iterator_t*) address;
    zend_hash_internal_pointer_reset_ex(
        &iterator->node->data.dir.children,
        &iterator->position);
    return em_vfs_iterator_update(iterator);
}

void EMSCRIPTEN_KEEPALIVE
    em_vfs_iterator_free(void* address) {
    em_vfs_iterator_t* iterator =
        (em_vfs_iterator_t*) address;
    pefree(iterator, 1);
}