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

#ifdef ZTS
#include <TSRM.h>
#endif

#include <SAPI.h>

#include <php_main.h>
#include <zend_exceptions.h>
#include <ext/standard/php_filestat.h>
#include <sys/types.h>

#include "node.h"
#include "path.h"
#include "dir.h"
#include "vfs.h"

#ifdef HAVE_EM_SQLITE_VFS
extern void em_sqlite_vfs_register(void);
extern void em_sqlite_vfs_unregister(void);
#endif

static php_stream_wrapper em_vfs_wrapper;
static php_stream_ops     em_vfs_ops;

static em_vfs_node_t* em_vfs = NULL;

em_vfs_node_t* em_vfs_resolve(em_vfs_path_t* vpath, bool make) {
    if (!em_vfs) {
        return NULL;
    }

    if (!vpath ||
        !vpath->directory ||
        (strcmp(vpath->directory, "/") == SUCCESS) ||
        (strlen(vpath->directory) == 0)) {
        return em_vfs;
    }

    const char* cleaned = vpath->directory;
    if (cleaned[0] == '/') {
        cleaned++;
    }

    char* scanning = estrdup(cleaned);
    char* token = strtok(scanning, "/");

    em_vfs_node_t* current = em_vfs;
    while (token && current) {
        if (current->kind != EM_VFS_DIR) {
            efree(scanning);
            return NULL;
        }

        zval* child = zend_hash_str_find(
            &current->data.dir.children, token, strlen(token));

        if (child) {
            current = (em_vfs_node_t*)Z_PTR_P(child);
        } else if (make) {
            current = em_vfs_node_mkdir(current, token);
        } else {
            current = NULL;
        }

        token = strtok(NULL, "/");
    }
 
    efree(scanning);
    return current;
}

zend_result em_vfs_stat_path(em_vfs_path_t* vpath, php_stream_statbuf *ssb, bool link) {
    // If no filename specified, we're statting the directory itself
    if (!vpath->filename || strlen(vpath->filename) == 0) {
        em_vfs_node_t* directory = em_vfs_resolve(vpath, false);
        if (!directory) {
            return FAILURE;
        }
        return em_vfs_node_stat(directory, ssb, link);
    }

    // Look for file/subdirectory in parent directory
    em_vfs_node_t* parent = em_vfs_resolve(vpath, false);
    if (!parent || parent->kind != EM_VFS_DIR) {
        return FAILURE;
    }

    em_vfs_node_t* node = (em_vfs_node_t*)
        zend_hash_str_find_ptr(
            &parent->data.dir.children,
            vpath->filename, strlen(vpath->filename));

    if (!node) {
        return FAILURE;
    }

    return em_vfs_node_stat(node, ssb, link);
}

ssize_t em_vfs_mount(em_vfs_path_t* vpath, const char* mode, em_vfs_abstract_t* abstract) {
    // Determine mode flags
    bool want_read = strchr(mode, 'r') != NULL;
    bool want_write = strchr(mode, 'w') != NULL;

    // Resolve parent directory (create if needed for write mode)
    em_vfs_node_t* parent = em_vfs_resolve(vpath, want_write);
    if (!parent || parent->kind != EM_VFS_DIR) {
        return FAILURE;
    }

    // Look for existing file
    em_vfs_node_t* node = (em_vfs_node_t*)
        zend_hash_str_find_ptr(
            &parent->data.dir.children,
            vpath->filename, strlen(vpath->filename));

    if (want_read && !want_write) {
        // Read-only
        if (node && node->kind == EM_VFS_FILE) {
            abstract->node = em_vfs_node_copy(node);
            abstract->maximum = node->data.file.size;
            abstract->data = pecalloc(
                sizeof(char), abstract->maximum, 1);
            memcpy(abstract->data,
                node->data.file.content,
                node->data.file.size);
            abstract->length = node->data.file.size;
            return abstract->length;
        }
        return FAILURE;
    }

    if (want_write) {
        if (!node) {
            // nothing to write
            node = em_vfs_node_mkfile(
                parent, vpath->filename);
        }

        if (!node) {
            // nothing to do
            return FAILURE;
        }

        abstract->node = em_vfs_node_copy(node);

        // If also want_read and file exists, load content; else start empty
        if (want_read && node->data.file.size > 0) {
            abstract->maximum =
                node->data.file.size > 8192 ?
                    node->data.file.size : 8192;
            abstract->data = pecalloc(sizeof(char), abstract->maximum, 1);
            memcpy(abstract->data,
                   node->data.file.content,
                   node->data.file.size);
            abstract->length = node->data.file.size;
        } else {
            abstract->maximum = 8192;
            abstract->data = pecalloc(sizeof(char), abstract->maximum, 1);
            abstract->length = 0;
        }
        return SUCCESS;
    }

    return FAILURE;
}

ssize_t em_vfs_truncate(em_vfs_abstract_t* abstract, size_t count) {
    if (!abstract) {
        return -1;
    }
    if (count == abstract->length) {
        return count;
    }
    if (count < abstract->length) {
        // Shrink: just update length
        abstract->length = count;
        if (abstract->position > count) {
            abstract->position = count;
        }
        return count;
    }
    // Expand: reallocate if needed, zero-fill new space
    if (count > abstract->maximum) {
        size_t maximum = abstract->maximum;
        while (maximum < count) {
            maximum *= 2;
        }
        abstract->data = perealloc(abstract->data, maximum, 1);
        abstract->maximum = maximum;
    }
    memset(abstract->data + abstract->length, 0, count - abstract->length);
    abstract->length = count;
    return count;
}

ssize_t em_vfs_write_offset(em_vfs_abstract_t* abstract, const char* buffer, size_t count, size_t offset) {
    if ((offset + count) > abstract->maximum) {
        size_t maximum = abstract->maximum;
        while (maximum < (offset + count)) {
            maximum *= 2;  // Double the buffer size
        }
        abstract->data = perealloc(
            abstract->data, maximum, 1);
        abstract->maximum = maximum;
    }

    memcpy(abstract->data + offset, buffer, count);
    // Update position if this write is at/after current position
    if (offset + count > abstract->position) {
        abstract->position = offset + count;
    }
    // Update length if we wrote past the end
    if (offset + count > abstract->length) {
        abstract->length = offset + count;
    }
    return count;
}

ssize_t em_vfs_read_offset(em_vfs_abstract_t* abstract, char* buffer, size_t count, size_t offset) {
    // Nothing to read, return failure signal
    if (abstract->length <= 0) {
        return 0;
    }
    if (offset >= abstract->length) {
        return 0;
    }
    // Too much reading
    if (count > abstract->length - offset) {
        count = abstract->length - offset;
    }
    memcpy(buffer, &abstract->data[offset], count);
    // Optionally update position if this read is at/after current position
    if (offset + count > abstract->position) {
        abstract->position = offset + count;
    }
    return count;
}

static ssize_t em_vfs_stream_write(php_stream* stream, const char*buffer, size_t count) {
    em_vfs_abstract_t* abstract =
        (em_vfs_abstract_t*)
            stream->abstract;

    return em_vfs_write(abstract, buffer, count);
}

static ssize_t em_vfs_stream_read(php_stream *stream, char *buffer, size_t count) {
    em_vfs_abstract_t* abstract =
        (em_vfs_abstract_t*)
            stream->abstract;

    return em_vfs_read(abstract, buffer, count);
}

void em_vfs_release(em_vfs_abstract_t* abstract) {
    if (!abstract) {
        return;
    }

    em_vfs_node_release(abstract->node);

    if (abstract->data) {
        pefree(abstract->data, 1);
    }

    pefree(abstract, 1);
}

void em_vfs_close(em_vfs_abstract_t* abstract, bool sync) {
    // Save buffer content back to VFS file node
    if (abstract->node &&
        abstract->node->kind == EM_VFS_FILE) {
        // Free old content
        if (abstract->node->data.file.content) {
            pefree(abstract->node->data.file.content, 1);
        }

        // Save new content
        abstract->node->data.file.content  = abstract->data;
        abstract->node->data.file.size     = abstract->length;
        abstract->node->data.file.modified = time(NULL);

        if (sync) {
            // Perform sync
            abstract->length   = abstract->node->data.file.size;
            abstract->data     = pecalloc(
                sizeof(char), abstract->length, 1);
            memcpy(abstract->data,
                abstract->node->data.file.content,
                abstract->length);
            abstract->position = 0;
            return;
        }

        // Don't double free this
        abstract->data = NULL;
    }
}

static int em_vfs_stream_close(php_stream *stream, int type) {
    em_vfs_abstract_t* abstract =
        (em_vfs_abstract_t*) stream->abstract;

    // We never used this, points at garbage
    stream->orig_path = NULL;

    em_vfs_close(abstract, false);
    em_vfs_release(abstract);

    return 0;
}

zend_result em_vfs_seek(em_vfs_abstract_t* abstract, zend_off_t offset, int whence, zend_off_t *position) {
    if (!abstract) {
        return FAILURE;
    }

    switch (whence) {
        case SEEK_SET:
            (*position) = offset;
            break;
        case SEEK_CUR:
            (*position) = abstract->position + offset;
            break;
        case SEEK_END:
            (*position) = abstract->length + offset;
            break;
        default:
            return FAILURE;
    }

    // Clamp to valid range
    if ((*position) < 0) {
        (*position) = 0;
    } else if ((*position) > abstract->length) {
        (*position) = abstract->length;
    }

    abstract->position = (*position);

    return SUCCESS;
}

static int em_vfs_stream_seek(
    php_stream* stream, zend_off_t offset, int whence, zend_off_t *position) {
    return em_vfs_seek(stream->abstract, offset, whence, position);
}

static int em_vfs_stream_stat(php_stream *stream, php_stream_statbuf *ssb) {
    em_vfs_abstract_t* abstract =
        (em_vfs_abstract_t*)
            stream->abstract;
    
    if (!abstract) {
        return FAILURE;
    }

    return em_vfs_node_stat(abstract->node, ssb, 0);
}

static php_stream_ops em_vfs_ops = {
    em_vfs_stream_write,   // write
    em_vfs_stream_read,    // read  
    em_vfs_stream_close,   // close
    NULL,                  // flush
    "em-file",
    em_vfs_stream_seek,    // seek
    NULL,                  // cast
    em_vfs_stream_stat,    // stat
    NULL                   // set_option
};

em_vfs_abstract_t* em_vfs_open(
    const char* path,
    const char* mode) {
    em_vfs_path_t* vpath = em_vfs_mkpath(path);

    if (!vpath) {
        return NULL;
    }

    em_vfs_abstract_t* abstract = pecalloc(1, sizeof(em_vfs_abstract_t), 1);

    if (em_vfs_mount(
            vpath, mode, abstract) < 0) {
        em_vfs_release(abstract);
        em_vfs_path_release(vpath);
        return NULL;
    }

    em_vfs_path_release(vpath);
    return abstract;
}

static php_stream *em_vfs_wrapper_open(php_stream_wrapper *wrapper, 
                                  const char *path, const char *mode,
                                  int options, zend_string **opened_path,
                                  php_stream_context *context STREAMS_DC) {
    em_vfs_abstract_t* abstract = em_vfs_open(path, mode);
    if (!abstract) {
        return NULL;
    }

    if (opened_path) {
        *opened_path =
            zend_string_init(path, strlen(path), 0);
    }

    return php_stream_alloc(&em_vfs_ops, abstract, 0, mode);
}

static int em_vfs_wrapper_stat_stream(
    php_stream_wrapper *wrapper,
    php_stream *stream,
    php_stream_statbuf *ssb) {
    em_vfs_abstract_t* abstract =
        (em_vfs_abstract_t*)
            stream->abstract;

    if (!abstract) {
        return FAILURE;
    }

    return em_vfs_node_stat(abstract->node, ssb, 1);
}

static int em_vfs_wrapper_stat_uri(
    php_stream_wrapper *wrapper,
    const char *uri, int flags,
    php_stream_statbuf *ssb,
    php_stream_context *context) {
    em_vfs_path_t* vpath = em_vfs_mkpath(uri);
    if (!vpath) {
        return FAILURE;
    }

    int result =
        em_vfs_stat_path(vpath, ssb, 1);
    em_vfs_path_release(vpath);
    return result;
}

static int em_vfs_wrapper_unlink(
    php_stream_wrapper* wrapper,
    const char* path,
    int options, php_stream_context* context) {
    return em_vfs_unlink(path, false);
}

static int em_vfs_wrapper_rename(
    php_stream_wrapper* wrapper,
    const char* from,
    const char* to,
    int options, php_stream_context* context) {
    return em_vfs_move(from, to);
}

static int em_vfs_wrapper_mkdir(
    php_stream_wrapper* wrapper,
    const char* url,
    int mode, int options,
    php_stream_context* context) {
    return em_vfs_mkdir(url);
}

static int em_vfs_wrapper_rmdir(
    php_stream_wrapper* wrapper,
    const char* url, int options, 
    php_stream_context* context) {
    return em_vfs_unlink(url, true);
}

static php_stream_wrapper_ops em_vfs_wrapper_ops = {
    em_vfs_wrapper_open,
    NULL,
    em_vfs_wrapper_stat_stream,
    em_vfs_wrapper_stat_uri,
    em_vfs_wrapper_opendir,
    "em-vfs",
    em_vfs_wrapper_unlink,
    em_vfs_wrapper_rename,
    em_vfs_wrapper_mkdir,
    em_vfs_wrapper_rmdir,
    NULL,
};

static php_stream_wrapper em_vfs_wrapper = {
    &em_vfs_wrapper_ops,
    NULL,
    0
};

void em_vfs_startup(void) {
    em_vfs = pecalloc(1, sizeof(em_vfs_node_t), 1);
    em_vfs->kind = EM_VFS_DIR;
    em_vfs->name = pestrdup("/", 1);
    em_vfs->parent = NULL;
    em_vfs->data.dir.created = time(NULL);
    em_vfs->refcount = 1;
    zend_hash_init(
        &em_vfs->data.dir.children, 8,
        NULL, em_vfs_node_dtor, 1);
#ifdef HAVE_EM_SQLITE_VFS
    em_sqlite_vfs_register();
#endif
}

void em_vfs_activate(void) {
    php_unregister_url_stream_wrapper("file");
    php_register_url_stream_wrapper("vfs", &em_vfs_wrapper);
    php_register_url_stream_wrapper_volatile(
        ZSTR_KNOWN(ZEND_STR_FILE), &em_vfs_wrapper);
}

void em_vfs_deactivate(void) {
    php_unregister_url_stream_wrapper_volatile(
        ZSTR_KNOWN(ZEND_STR_FILE));
    php_unregister_url_stream_wrapper("vfs");
}

void em_vfs_shutdown(void) {
#ifdef HAVE_EM_SQLITE_VFS
    em_sqlite_vfs_unregister();
#endif

    em_vfs_node_release(em_vfs);
}

bool EMSCRIPTEN_KEEPALIVE
    em_vfs_put(const char* path, const char* data, size_t length) {
    em_vfs_path_t* vpath = em_vfs_mkpath(path);

    if (!vpath) {
        return NULL;
    }

    em_vfs_abstract_t* abstract = pecalloc(1, sizeof(em_vfs_abstract_t), 1);

    if (em_vfs_mount(
            vpath, "w", abstract) < 0) {
        em_vfs_release(abstract);
        em_vfs_path_release(vpath);
        return false;
    }

    em_vfs_path_release(vpath);

    if (em_vfs_write(abstract, data, length) < 0) {
        em_vfs_release(abstract);
        return false;
    }

    em_vfs_close(abstract, false);
    em_vfs_release(abstract);
    return true;
}

void* EMSCRIPTEN_KEEPALIVE
    em_vfs_get_address(const char* path) {
    em_vfs_path_t* vpath = em_vfs_mkpath(path);

    if (!vpath) {
        return NULL;
    }

    em_vfs_node_t* parent =
        em_vfs_resolve(vpath, false);

    if (!parent) {
        em_vfs_path_release(vpath);
        return NULL;
    }

    em_vfs_node_t* node = (em_vfs_node_t*)
        zend_hash_str_find_ptr(
            &parent->data.dir.children,
            vpath->filename, strlen(vpath->filename));
    em_vfs_path_release(vpath);

    if (node->kind == EM_VFS_DIR) {
        return NULL;
    }

    return node->data.file.content;
}

ssize_t EMSCRIPTEN_KEEPALIVE
    em_vfs_get_length(const char* path) {
        em_vfs_path_t* vpath = em_vfs_mkpath(path);

    if (!vpath) {
        return -1;
    }

    em_vfs_node_t* parent =
        em_vfs_resolve(vpath, false);

    if (!parent) {
        em_vfs_path_release(vpath);
        return -1;
    }

    em_vfs_node_t* node = (em_vfs_node_t*)
        zend_hash_str_find_ptr(
            &parent->data.dir.children,
            vpath->filename, strlen(vpath->filename));
    em_vfs_path_release(vpath);

    if (!node) {
        return -1;
    }

    if (node->kind == EM_VFS_DIR) {
        return -1;
    }

    return node->data.file.size;
}

bool EMSCRIPTEN_KEEPALIVE em_vfs_unlink(const char* path, bool directories) {
    // Parse the path
    em_vfs_path_t* vpath = em_vfs_mkpath(path);
    if (!vpath) {
        return 0;
    }

    // Can't unlink empty filename or root
    if (!vpath->filename || strlen(vpath->filename) == 0) {
        em_vfs_path_release(vpath);
        return 0;
    }

    // Resolve parent directory
    em_vfs_node_t* parent = em_vfs_resolve(vpath, false);
    if (!parent || parent->kind != EM_VFS_DIR) {
        em_vfs_path_release(vpath);
        return 0;
    }

    // Check if file exists in parent directory
    em_vfs_node_t* node = zend_hash_str_find_ptr(
        &parent->data.dir.children,
        vpath->filename, strlen(vpath->filename));

    if (!node) {
        // File doesn't exist
        em_vfs_path_release(vpath);
        return 0;
    }

    // Can only unlink files, not directories
    if (!directories && node->kind != EM_VFS_FILE) {
        em_vfs_path_release(vpath);
        return 0;
    }

    // Remove the file from children hash
    zend_result result = zend_hash_str_del(
        &parent->data.dir.children,
        vpath->filename, strlen(vpath->filename));

    if (result == SUCCESS) {
        php_clear_stat_cache(0,
            vpath->filename,
            strlen(vpath->filename));
    }

    em_vfs_path_release(vpath);

    return (result == SUCCESS);
}

bool EMSCRIPTEN_KEEPALIVE
    em_vfs_mkdir(const char* path) {
    em_vfs_path_t* vpath =
        em_vfs_mkpath(path);

    if (!vpath) {
        return false;
    }

    if (em_vfs_resolve(vpath, false)) {
        em_vfs_path_release(vpath);
        return false;
    }

    em_vfs_node_t* node =
        em_vfs_resolve(vpath, true);
    em_vfs_path_release(vpath);
    return node != NULL;
}

bool EMSCRIPTEN_KEEPALIVE em_vfs_move(const char* from, const char* to) {
    em_vfs_path_t* from_path = em_vfs_mkpath(from);
    em_vfs_path_t* to_path = em_vfs_mkpath(to);

    if (!from_path || !to_path) {
        if (from_path) em_vfs_path_release(from_path);
        if (to_path) em_vfs_path_release(to_path);
        return false;
    }

    // Can't move root or empty filenames
    if (!from_path->filename || strlen(from_path->filename) == 0 ||
        !to_path->filename || strlen(to_path->filename) == 0) {
        em_vfs_path_release(from_path);
        em_vfs_path_release(to_path);
        return false;
    }

    // Get source parent and node
    em_vfs_node_t* from_parent = em_vfs_resolve(from_path, false);
    if (!from_parent || from_parent->kind != EM_VFS_DIR) {
        em_vfs_path_release(from_path);
        em_vfs_path_release(to_path);
        return false;
    }
    
    em_vfs_node_t* node = zend_hash_str_find_ptr(
        &from_parent->data.dir.children,
        from_path->filename, strlen(from_path->filename));
    
    if (!node) {
        em_vfs_path_release(from_path);
        em_vfs_path_release(to_path);
        return false;
    }
    
    // Get destination parent (create if needed)
    em_vfs_node_t* to_parent = em_vfs_resolve(to_path, true);
    if (!to_parent || to_parent->kind != EM_VFS_DIR) {
        em_vfs_path_release(from_path);
        em_vfs_path_release(to_path);
        return false;
    }

    // Check if destination already exists
    em_vfs_node_t* existing = zend_hash_str_find_ptr(
        &to_parent->data.dir.children,
        to_path->filename, strlen(to_path->filename));
    
    if (existing) {
        em_vfs_path_release(from_path);
        em_vfs_path_release(to_path);
        return false;
    }

    // Update the node's name and parent
    if (node->name) {
        pefree(node->name, 1);
    }
    node->name = pestrdup(to_path->filename, 1);

    // Update parent reference
    if (node->parent) {
        em_vfs_node_release(node->parent);
    }
    node->parent = em_vfs_node_copy(to_parent);
    
    // Add to destination parent
    zend_hash_str_add_ptr(
        &to_parent->data.dir.children,
        to_path->filename, strlen(to_path->filename),
        em_vfs_node_copy(node));

    // Remove from source parent
    zend_hash_str_del(
        &from_parent->data.dir.children,
        from_path->filename, strlen(from_path->filename));
    
    // Update modification time if it's a file
    if (node->kind == EM_VFS_FILE) {
        node->data.file.modified = time(NULL);
    }
    
    em_vfs_path_release(from_path);
    em_vfs_path_release(to_path);
    
    return true;
}

void EMSCRIPTEN_KEEPALIVE
    em_vfs_reset(void) {
        em_vfs_shutdown();
        em_vfs_startup();
}