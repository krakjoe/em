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

#include <php.h>

#ifdef ZTS
#include <TSRM.h>
#endif

#ifdef EMSCRIPTEN
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include <SAPI.h>

#include <php_main.h>
#include <zend_exceptions.h>
#include <ext/standard/php_filestat.h>

static php_stream_wrapper em_vfs_wrapper;
static php_stream_ops     em_vfs_ops;

typedef struct _em_vfs_node_t em_vfs_node_t;

typedef struct _em_vfs_abstract_t {
    char*               data;
    ssize_t             length;
    size_t              position;
    size_t              maximum;
    em_vfs_node_t*      node;
} em_vfs_abstract_t;

typedef enum _em_vfs_node_kind_t {
    EM_VFS_DIR,
    EM_VFS_FILE,
} em_vfs_node_kind_t;

struct _em_vfs_node_t {
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

typedef struct _em_vfs_dir_abstract_t {
    em_vfs_node_t* directory;
    HashPosition   position;
    bool           started;
} em_vfs_dir_abstract_t;

static em_vfs_node_t* em_vfs = NULL;

typedef struct _em_vfs_path_t {
    char* original;     // Full original path: "vfs://dir/file.txt"
    char* directory;    // Directory part: "dir" or "" for root
    char* filename;     // Filename part: "file.txt"
    bool is_root;       // True if directory is root
} em_vfs_path_t;

static em_vfs_path_t* em_vfs_mkpath(const char* path) {
    if (!path) {
        return NULL;
    }

    em_vfs_path_t* vpath = pecalloc(1, sizeof(em_vfs_path_t), 1);

    // Store original path
    vpath->original = pestrdup(path, 1);

    // Skip vfs:// prefix if present
    const char* clean_path = path;
    if (strncmp(path, "vfs://", 6) == 0) {
        clean_path = path + 6;
    }

    // Handle empty path (root directory)
    if (!clean_path || *clean_path == '\0') {
        vpath->directory = pestrdup("", 1);
        vpath->filename = pestrdup("", 1);
        vpath->is_root = true;
        return vpath;
    }

    // Find last slash
    char* temp_path = estrdup(clean_path);
    char* last_slash = strrchr(temp_path, '/');

    if (last_slash) {
        // Path has directory: "dir/file.txt"
        *last_slash = '\0';
        vpath->directory = pestrdup(temp_path, 1);
        vpath->filename = pestrdup(last_slash + 1, 1);
        vpath->is_root = (strlen(vpath->directory) == 0);
    } else {
        // Path is just filename: "file.txt"
        vpath->directory = pestrdup("", 1);
        vpath->filename = pestrdup(temp_path, 1);
        vpath->is_root = true;
    }

    efree(temp_path);
    return vpath;
}

static void em_vfs_path_release(em_vfs_path_t* vpath) {
    if (!vpath) {
        return;
    }
    
    if (vpath->original) {
        pefree(vpath->original, 1);
    }
    if (vpath->directory) {
        pefree(vpath->directory, 1);
    }
    if (vpath->filename) {
        pefree(vpath->filename, 1);
    }
    
    pefree(vpath, 1);
}

static void em_vfs_node_release(em_vfs_node_t* node) {
    if (node->name) {
        pefree(node->name, 1);
    }

    if (node->kind == EM_VFS_FILE) {
        if (node->data.file.content) {
            pefree(node->data.file.content, 1);
        }
    } else if (node->kind == EM_VFS_DIR) {
        zend_hash_destroy(&node->data.dir.children);
    }

    pefree(node, 1);
}

static void em_vfs_node_dtor(zval *zv) {
    em_vfs_node_t* node =
        (em_vfs_node_t*)Z_PTR_P(zv);
    if (node) {
        em_vfs_node_release(node);
    }
}

static em_vfs_node_t* em_vfs_mkfile(em_vfs_node_t* parent, const char* name) {
    em_vfs_node_t* file = pecalloc(1, sizeof(em_vfs_node_t), 1);
    file->kind = EM_VFS_FILE;
    file->name = pestrdup(name, 1);
    file->parent = parent;
    file->data.file.created = time(NULL);
    file->data.file.modified = time(NULL);
    zend_hash_str_add_ptr(
        &parent->data.dir.children,
        name, strlen(name), file);
    return file;
}

static em_vfs_node_t* em_vfs_mkdir(em_vfs_node_t* parent, const char* name) {
    em_vfs_node_t* dir = pecalloc(1, sizeof(em_vfs_node_t), 1);
    dir->kind = EM_VFS_DIR;
    dir->name = pestrdup(name, 1);
    dir->parent = parent;
    dir->data.dir.created = time(NULL);
    zend_hash_init(
        &dir->data.dir.children, 8, NULL,
        em_vfs_node_dtor, 1);
    zend_hash_str_add_ptr(
        &parent->data.dir.children,
        name, strlen(name), dir);    
    return dir;
}

static em_vfs_node_t* em_vfs_resolve(em_vfs_path_t* vpath, bool make) {
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
            current = em_vfs_mkdir(current, token);
        } else {
            current = NULL;
        }

        token = strtok(NULL, "/");
    }
 
    efree(scanning);
    return current;
}

static zend_always_inline zend_result em_vfs_stat_node(em_vfs_node_t* node, php_stream_statbuf *ssb, bool link) {
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

static zend_result em_vfs_stat(em_vfs_path_t* vpath, php_stream_statbuf *ssb, bool link) {
    // If no filename specified, we're statting the directory itself
    if (!vpath->filename || strlen(vpath->filename) == 0) {
        em_vfs_node_t* directory = em_vfs_resolve(vpath, false);
        if (!directory) {
            return FAILURE;
        }
        return em_vfs_stat_node(directory, ssb, link);
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

    return em_vfs_stat_node(node, ssb, link);
}

static ssize_t em_vfs_mount(em_vfs_path_t* vpath, const char* mode, em_vfs_abstract_t* abstract) {
    // Resolve parent directory (create if needed for write mode)
    em_vfs_node_t* parent = em_vfs_resolve(vpath, strchr(mode, 'w') != NULL);
    if (!parent || parent->kind != EM_VFS_DIR) {
        return FAILURE;
    }

    // Look for existing file
    em_vfs_node_t* node = (em_vfs_node_t*)
        zend_hash_str_find_ptr(
            &parent->data.dir.children,
            vpath->filename, strlen(vpath->filename));

    if (strchr(mode, 'r')) {
        if (node && node->kind == EM_VFS_FILE) {
            abstract->node = node;
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

    if (strchr(mode, 'w')) {
        if (!node) {
            node = em_vfs_mkfile(parent, vpath->filename);
        }
        if (!node) {
            return FAILURE;
        }

        abstract->node = node;
        abstract->maximum = 8192;
        abstract->data = pecalloc(sizeof(char), abstract->maximum, 1);
        abstract->length = 0;
        return SUCCESS;
    }

    return FAILURE;
}

static ssize_t em_vfs_stream_write(php_stream* stream, const char*buffer, size_t count) {
    em_vfs_abstract_t* abstract =
        (em_vfs_abstract_t*)
            stream->abstract;

    if ((abstract->position + count) > abstract->maximum) {
        size_t maximum = abstract->maximum, old_max = maximum;
        while (maximum < (abstract->position + count)) {
            maximum *= 2;  // Double the buffer size
        }

        abstract->data = perealloc(
            abstract->data, maximum, 1);
        abstract->maximum = maximum;
    }

    memcpy(abstract->data + abstract->position, buffer, count);
    abstract->position += count;

    // Update length if we wrote past the end
    if (abstract->position > abstract->length) {
        abstract->length = abstract->position;
    }

    return count;
}

static ssize_t em_vfs_stream_read(php_stream *stream, char *buffer, size_t count) {
    em_vfs_abstract_t* abstract =
        (em_vfs_abstract_t*)
            stream->abstract;

    // Nothing to read, return failure signal
    if (abstract->length <= 0) {
        return abstract->length;
    }

    // Nothing left to read, return EOF
    if (abstract->position == abstract->length) {
        return EOF;
    }

    /// Too much reading
    if (count > abstract->length - abstract->position) {
        count = abstract->length - abstract->position;
    }

    memcpy(buffer,
        &abstract->data[abstract->position],
        count);

    abstract->position += count;

    return count;
}

static void em_vfs_abstract_release(em_vfs_abstract_t* abstract) {
    if (!abstract) {
        return;
    }

    if (abstract->data) {
        pefree(abstract->data, 1);
    }

    pefree(abstract, 1);
}

static int em_vfs_stream_close(php_stream *stream, int type) {
    em_vfs_abstract_t* abstract =
        (em_vfs_abstract_t*) stream->abstract;

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
        
        // Don't double free this
        abstract->data = NULL;
    }

    // We never used this, points at garbage
    stream->orig_path = NULL;

    em_vfs_abstract_release(abstract);
    return 0;
}

static int em_vfs_stream_seek(
    php_stream* stream, zend_off_t offset, int whence, zend_off_t *position) {
    em_vfs_abstract_t* abstract =
        (em_vfs_abstract_t*)
            stream->abstract;

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

static int em_vfs_stream_stat(php_stream *stream, php_stream_statbuf *ssb) {
    em_vfs_abstract_t* abstract =
        (em_vfs_abstract_t*)
            stream->abstract;
    
    if (!abstract) {
        return FAILURE;
    }

    return em_vfs_stat_node(abstract->node, ssb, 0);
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

static php_stream *em_vfs_wrapper_open(php_stream_wrapper *wrapper, 
                                  const char *path, const char *mode,
                                  int options, zend_string **opened_path,
                                  php_stream_context *context STREAMS_DC) {
    em_vfs_path_t* vpath = em_vfs_mkpath(path);

    if (!vpath) {
        return NULL;
    }

    em_vfs_abstract_t* abstract = pecalloc(1, sizeof(em_vfs_abstract_t), 1);

    if (em_vfs_mount(
            vpath, mode, abstract) < 0) {
        em_vfs_abstract_release(abstract);
        em_vfs_path_release(vpath);
        return NULL;
    }

    em_vfs_path_release(vpath);

    *opened_path = zend_string_init(path, strlen(path), 0);

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

    return em_vfs_stat_node(abstract->node, ssb, 1);
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
        em_vfs_stat(vpath, ssb, 1);
    em_vfs_path_release(vpath);
    return result;
}

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

static php_stream* em_vfs_wrapper_opendir(
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

static int em_vfs_wrapper_unlink(
    php_stream_wrapper* wrapper,
    const char* url,
    int options, php_stream_context* context) {
    
    // Parse the path
    em_vfs_path_t* vpath = em_vfs_mkpath(url);
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
    if (node->kind != EM_VFS_FILE) {
        em_vfs_path_release(vpath);
        return 0;
    }

    // Remove the file from children hash
    zend_result result = zend_hash_str_del(
        &parent->data.dir.children,
        vpath->filename, strlen(vpath->filename));

    em_vfs_path_release(vpath);

    if (result == SUCCESS) {
        php_clear_stat_cache(0,
            vpath->filename,
            strlen(vpath->filename));
    }

    return (result == SUCCESS);
}

static int em_vfs_wrapper_rename(
    php_stream_wrapper* wrapper,
    const char* from,
    const char* to,
    int options, php_stream_context* context) {
    return 0;
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

    zend_hash_init(
        &em_vfs->data.dir.children, 8,
        NULL, em_vfs_node_dtor, 1);
}

void em_vfs_activate(void) {
    php_register_url_stream_wrapper("vfs",   &em_vfs_wrapper);
}

void em_vfs_deactivate(void) {
    php_unregister_url_stream_wrapper("vfs");
}

void em_vfs_shutdown(void) {
    if (!em_vfs) {
        return;
    }

    em_vfs_node_release(em_vfs);
}

void EMSCRIPTEN_KEEPALIVE
    em_vfs_reset(void) {
        em_vfs_shutdown();
        em_vfs_startup();
}