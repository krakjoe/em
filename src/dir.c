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
#include "path.h"
#include "dir.h"
#include "vfs.h"

typedef struct _em_vfs_dir_abstract_t {
    em_vfs_node_t* directory;
    HashPosition   position;
    bool           started;
} em_vfs_dir_abstract_t;

static ssize_t em_vfs_dir_read(php_stream *stream, char *buffer, size_t count) {
    em_vfs_dir_abstract_t* abstract =
        (em_vfs_dir_abstract_t*)
            stream->abstract;

    if (!abstract->started) {
        zend_hash_internal_pointer_reset_ex(
            &abstract->directory->data.dir.children,
            &abstract->position);
        abstract->started = true;
    }

    zval *entry = NULL;
    zend_string *key;
    zend_ulong idx;

    if ((entry = zend_hash_get_current_data_ex(
            &abstract->directory->data.dir.children,
                &abstract->position)) &&
        (zend_hash_get_current_key_ex(
            &abstract->directory->data.dir.children,
                &key, &idx, &abstract->position) == HASH_KEY_IS_STRING)) {
        php_stream_dirent dent;
        em_vfs_node_t* node = Z_PTR_P(entry);

        memcpy(
            dent.d_name,
            ZSTR_VAL(key),
            ZSTR_LEN(key));
        dent.d_name[ZSTR_LEN(key)]=0;
#if PHP_VERSION_ID >= 80300
        dent.d_type = (node->kind == EM_VFS_FILE) ?
            DT_REG : DT_DIR;
#endif
        memcpy(buffer, &dent, sizeof(php_stream_dirent));

        zend_hash_move_forward_ex(
            &abstract->directory->data.dir.children,
            &abstract->position);

        return sizeof(php_stream_dirent);
    }

    return FAILURE;
}

static int em_vfs_dir_close(php_stream *stream, int type) {
    em_vfs_dir_abstract_t* abstract =
        (em_vfs_dir_abstract_t*)
            stream->abstract;

    if (abstract) {
        pefree(abstract, 1);
    }

    return SUCCESS;
}

static int em_vfs_dir_rewind(php_stream* stream, zend_off_t offset, int whence, zend_off_t *position) {
    (void) offset;
    (void) whence;

    em_vfs_dir_abstract_t* abstract =
        (em_vfs_dir_abstract_t*)
            stream->abstract;

    if (!abstract) {
        return FAILURE;
    }

    // Reset the hash table position to the beginning
    zend_hash_internal_pointer_reset_ex(
        &abstract->directory->data.dir.children,
        &abstract->position);

    // Mark as not started 
    //  so next read will begin from start
    abstract->started  = false;

    // Set position to 0
    if (position) {
        *position = 0;
    }
    
    return SUCCESS;
}

static php_stream_ops em_vfs_dir_ops = {
    NULL,                // write
    em_vfs_dir_read,     // read  
    em_vfs_dir_close,    // close
    NULL,                // flush
    "em-vfs-dir",
    em_vfs_dir_rewind,   // seek
    NULL,                // cast
    NULL,                // stat
    NULL                 // set_option
};

php_stream* em_vfs_wrapper_opendir(
    php_stream_wrapper* wrapper,
    const char* filename,
    const char* mode,
    int options,
    zend_string **opened_path,
    php_stream_context* context STREAMS_DC) {
    
    em_vfs_path_t* vpath = em_vfs_mkpath(filename);
    if (!vpath) {
        return NULL;
    }
    
    fprintf(stderr, "opendir(%s)\n", filename);

    em_vfs_node_t* directory = NULL;
    
    // If no filename part, we want the directory itself
    if (!vpath->filename || strlen(vpath->filename) == 0) {
        directory = em_vfs_resolve(vpath, false);
    } else {
        // We have a filename part - check if it's a directory in the parent
        em_vfs_node_t* parent = em_vfs_resolve(vpath, false);
        if (parent && parent->kind == EM_VFS_DIR) {
            directory = (em_vfs_node_t*)zend_hash_str_find_ptr(
                &parent->data.dir.children,
                vpath->filename, strlen(vpath->filename));
        }
    }
    
    em_vfs_path_release(vpath);
    
    if (!directory || directory->kind != EM_VFS_DIR) {
        return NULL;
    }

    em_vfs_dir_abstract_t* abstract = pecalloc(
        1, sizeof(em_vfs_dir_abstract_t), 1);
    abstract->directory = directory;
    abstract->started = false;

    if (opened_path) {
        *opened_path = zend_string_init(filename, strlen(filename), 0);
    }

    return php_stream_alloc(&em_vfs_dir_ops, abstract, 0, mode);
}