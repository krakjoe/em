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

#include "node.h"

zend_result em_vfs_node_stat(em_vfs_node_t* node, php_stream_statbuf *ssb, bool link) {
    memset(ssb, 0, sizeof(php_stream_statbuf));

    if (!node) {
        return FAILURE;
    }

    if (node->kind == EM_VFS_FILE) { // File
        ssb->sb.st_size = node->data.file.size;
        ssb->sb.st_mode = S_IFREG | 0644;
        ssb->sb.st_mtime = node->data.file.modified;
        ssb->sb.st_ctime = node->data.file.created;
        ssb->sb.st_atime = node->data.file.modified;
    } else {                         // Directory
        ssb->sb.st_size = 0;
        ssb->sb.st_mode = S_IFDIR | 0755;
        ssb->sb.st_mtime = node->data.dir.created;
        ssb->sb.st_ctime = node->data.dir.created;
        ssb->sb.st_atime = node->data.dir.created;
    }

    ssb->sb.st_nlink = link;
    ssb->sb.st_uid   = 0;
    ssb->sb.st_gid   = 0;

    return SUCCESS;
}

em_vfs_node_t* em_vfs_node_mkfile(em_vfs_node_t* parent, const char* name) {
    em_vfs_node_t* file = calloc(1, sizeof(em_vfs_node_t));
    file->kind = EM_VFS_FILE;
    file->name = strdup(name);
    file->parent = em_vfs_node_copy(parent);
    file->data.file.created  = time(NULL);
    file->data.file.modified = time(NULL);
    file->data.file.size     = 0;
    file->data.file.content  = NULL;
    zend_hash_str_add_ptr(
        &parent->data.dir.children,
        name, strlen(name), file);
    file->refcount = 1;
    return file;
}

em_vfs_node_t* em_vfs_node_mkdir(em_vfs_node_t* parent, const char* name) {
    em_vfs_node_t* dir = calloc(1, sizeof(em_vfs_node_t));
    dir->kind = EM_VFS_DIR;
    dir->name = strdup(name);
    dir->parent = em_vfs_node_copy(parent);
    dir->data.dir.created  = time(NULL);
    zend_hash_init(
        &dir->data.dir.children, 8, NULL,
        em_vfs_node_dtor, 1);
    zend_hash_str_add_ptr( 
        &parent->data.dir.children,
        name, strlen(name), dir);  
    dir->refcount = 1;  
    return dir;
}

void em_vfs_node_free(em_vfs_node_t* node) {
    if (node->parent) {
        em_vfs_node_release(node->parent);
    }

    if (node->name) {
        free(node->name);
    }

    if (node->kind == EM_VFS_FILE) {
        if (node->data.file.content) {
            free(node->data.file.content);
        }
    } else if (node->kind == EM_VFS_DIR) {
        zend_hash_destroy(&node->data.dir.children);
    }

    free(node);

    node = NULL;
}

void em_vfs_node_release(em_vfs_node_t* node) {
    if (!node) {
        return;
    }

    if (--node->refcount) {
        return;
    }

    em_vfs_node_free(node);
}

void em_vfs_node_dtor(zval *zv) {
    em_vfs_node_t* node =
        (em_vfs_node_t*)Z_PTR_P(zv);
    if (node) {
        em_vfs_node_release(node);
    }
}