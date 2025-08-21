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
#include "memory.h"

typedef struct _em_vfs_memory_version_t {
    int      major;
    int      minor;
    int      patch;
    uint64_t combined;
} em_vfs_memory_version_t;

static em_vfs_memory_entry_flags_t
    em_vfs_memory_data_flags(
        em_vfs_node_t *node) {
    if (node->kind == EM_VFS_DIR) {
        return EM_VFS_MEMORY_VERBATIM;
    }
#ifdef HAVE_EM_ZLIB
    if (node->data.file.size < EM_VFS_MEMORY_ZLIB_MIN) {
        return EM_VFS_MEMORY_VERBATIM;
    }
    return EM_VFS_MEMORY_COMPRESSED;
#endif
    return EM_VFS_MEMORY_VERBATIM;
}

static em_vfs_memory_entry_size_t em_vfs_memory_data_length(em_vfs_node_t* node) {
    if (node->kind == EM_VFS_DIR) {
        return (em_vfs_memory_entry_size_t) {0, 0};
    }

#ifdef HAVE_EM_ZLIB
    if (node->data.file.size < EM_VFS_MEMORY_ZLIB_MIN) {
        return (em_vfs_memory_entry_size_t) {
            .verbatim = node->data.file.size,
            .compressed = 0
        };
    }

    return (em_vfs_memory_entry_size_t) {
        .verbatim   = node->data.file.size,
        .compressed = compressBound(
            node->data.file.size)
    };
#endif
    return (em_vfs_memory_entry_size_t) {
        .verbatim = node->data.file.size,
        .compressed = 0
    };
}

static void em_vfs_memory_data_free(em_vfs_node_t* node, em_vfs_memory_entry_t* entry, void* copy) {
#ifdef HAVE_EM_ZLIB
    if (node->data.file.size < EM_VFS_MEMORY_ZLIB_MIN) {
        /* compression was never attempted */
        return;
    }

    if (!(entry->flags & EM_VFS_MEMORY_COMPRESSED)) {
        /* compression recovered from errors */
        return;
    }

    free(copy);
#endif
}

static void* em_vfs_memory_data_alloc(em_vfs_node_t* node, em_vfs_memory_entry_t *entry) {
#ifdef HAVE_EM_ZLIB
    if (node->data.file.size < EM_VFS_MEMORY_ZLIB_MIN) {
        return node->data.file.content;
    }

    void* copy = malloc(entry->size.data.compressed);

    if (!copy) {
        fprintf(stderr,
            "[memory] %s failed to allocate for compression\n",
            node->name);
        goto __em_vfs_memory_data_alloc_recover;
    }

    if (compress2(
        (Bytef*) copy,
            (uLongf*) &entry->size.data.compressed,
        (const Bytef *) node->data.file.content,
            node->data.file.size,
        EM_VFS_MEMORY_ZLIB_LEVEL) != Z_OK) {
        goto __em_vfs_memory_data_alloc_recover;
    }

    return copy;

__em_vfs_memory_data_alloc_recover: {
        /* recover from compression errors */
        if (copy) {
            free(copy);
        }
        entry->flags &= ~EM_VFS_MEMORY_COMPRESSED;
    }
#endif
    return node->data.file.content;
}

static size_t em_vfs_memory_path_length(em_vfs_node_t* node) {
    size_t length = 1; // Leading '/'
    
    // Build path and count actual length, skipping root "/"
    em_vfs_node_t* current = node;
    do {
        if ((current->parent == NULL) &&
            (strcmp(current->name, "/") == SUCCESS)) {
            break; // Skip root directory
        }
        length += strlen(current->name);
        if (current->parent &&
            current->parent->parent) {
            length += 1; // Add separator if not at root level
        }
    } while ((current = current->parent));
    
    return length + 1; // +1 for null terminator
}

static char* em_vfs_memory_path_alloc(em_vfs_node_t* node) {
    size_t length =
        em_vfs_memory_path_length(node);
    char* create = malloc(length);
    char* components[256];
    int depth = 0;

    em_vfs_node_t* current = node;
    do {
        if ((current->parent == NULL) &&
            (strcmp(current->name, "/") == SUCCESS)) {
            current = current->parent;
            break; // Skip root directory
        }
        components[depth++] = current->name;
    } while ((current = current->parent));

    create[0] = '\0';

    // Always start with "/" for absolute paths
    strcat(create, "/");

    for (int i = depth - 1; i >= 0; i--) {
        if (i < depth - 1) {
            strcat(create, "/");
        }
        strcat(create, components[i]);
    }

    return create;
}

/**
 * Shall write the node to entry, advancing the entry pointer and updating offsets in header
 */
static size_t 
    em_vfs_memory_data(
        em_vfs_memory_entry_t** entry, em_vfs_node_t* node, uint32_t* offsets, void* start) {
    // Record write count
    size_t written = 1;

    // Save the offset of this entry
    *offsets = (uint32_t)((char*)(*entry) - (char*)start);

    // Fill in the entry struct
    (*entry)->kind       = node->kind;
    (*entry)->flags      = em_vfs_memory_data_flags(node);
    (*entry)->reserved   = 0;
    (*entry)->size.entry = sizeof(em_vfs_memory_entry_t);
    (*entry)->size.name  = em_vfs_memory_path_length(node);
    (*entry)->size.data  = em_vfs_memory_data_length(node);
    (*entry)->stat.ctime =
        (node->kind == EM_VFS_FILE) ?
            node->data.file.created :
                node->data.dir.created;
    (*entry)->stat.mtime =
        (node->kind == EM_VFS_FILE) ?
            node->data.file.modified : 0;

    // Write the name (null-terminated)
    char *named = (char *)((*entry) + 1);
    char *alloced = em_vfs_memory_path_alloc(node);
    memcpy(named,
        alloced,
        (*entry)->size.name);
    free(alloced);

    // Increase entry size by name
    (*entry)->size.entry += (*entry)->size.name;

    // Write the data (for files)
    if ((node->kind == EM_VFS_FILE) &&
        (node->data.file.size > 0)) {
        void *data = named + (*entry)->size.name;
        void *copy =
            em_vfs_memory_data_alloc(
                node, (*entry));

        if ((*entry)->flags & EM_VFS_MEMORY_COMPRESSED) {
            memcpy(data, copy, (*entry)->size.data.compressed);
            (*entry)->size.entry +=
                (*entry)->size.data.compressed;
        } else {
            memcpy(data, copy, (*entry)->size.data.verbatim);
            (*entry)->size.entry +=
                (*entry)->size.data.verbatim;
        }

        em_vfs_memory_data_free(node, (*entry), copy);
    }

    // Advance entry pointer for next record
    *entry =
        (em_vfs_memory_entry_t *)
            ((char*)(*entry) + ((*entry)->size.entry));

    // Descend into directory for files if necessary
    if (node->kind == EM_VFS_DIR) {
        zend_string* name;
        em_vfs_node_t* child;
        size_t coffset = 1;

#ifdef HAVE_EM_ZLIB
        size_t compression = 0;
#endif

        ZEND_HASH_FOREACH_STR_KEY_PTR(
            &node->data.dir.children, name, child) {
            size_t cwritten =
                em_vfs_memory_data(
                    entry, child, &offsets[coffset], start);
            written += cwritten;
            coffset += cwritten;
#ifdef HAVE_EM_ZLIB
            if ((*entry)->flags & EM_VFS_MEMORY_COMPRESSED) {
                if (((compression +=
                        (*entry)->size.data.compressed) % 1024) == 0) {
                    emscripten_sleep(1);
                }
            } else 
#endif
            if ((written % 64) == 0) {
                emscripten_sleep(10);
            }
        } ZEND_HASH_FOREACH_END();
    }

    return written;
}

static zend_always_inline void
    em_vfs_memory_add(
        em_vfs_memory_header_t* destination,
        em_vfs_memory_header_t* source) {
    /* reserve offset addresses */
    destination->size.header  += source->size.header;
    /* increase stream length */
    destination->size.length  += source->size.length;
    /* adjust record count */
    destination->size.records += source->size.records;
}

static em_vfs_memory_header_t* em_vfs_memory_calculate(em_vfs_node_t* node) {
    if (node->kind != EM_VFS_DIR) {
        /* unreachable */
        return NULL;
    }

    /* Pass one, calculate for allocator */
    em_vfs_memory_header_t  calculator = {
        .magic   = "\0",
        .version = "\0",
        .size = {
            .header  = sizeof(uint32_t), /* this record offset in header */
            .length  = sizeof(em_vfs_memory_entry_t) +
                        em_vfs_memory_path_length(node),
                            /* minimal length of this entry */
            .records = 1, /* this is the first record */
        },
    };

    em_vfs_memory_header_t* result =
        pecalloc(1, sizeof(em_vfs_memory_header_t), 1);
    memcpy(result, &calculator, sizeof(em_vfs_memory_header_t));

    zend_string*   name;
    em_vfs_node_t* child;
    ZEND_HASH_FOREACH_STR_KEY_PTR(
        &node->data.dir.children, name, child) {
    
        /* adjust record count, directories will be counted by add */
        result->size.records++;

        /* reserve memory for entry */
        result->size.length +=
            sizeof(em_vfs_memory_entry_t) +      /* necessary for entry */
            (em_vfs_memory_path_length(child)); /* null terminated name */ 

        /* reserve memory for data */
        em_vfs_memory_entry_size_t size =
            em_vfs_memory_data_length(child);

        if (size.compressed) {
            result->size.length +=
                size.compressed;
        } else {
            result->size.length +=
                size.verbatim;
        }

        /* reserve memory at end of header for offset */
        result->size.header += sizeof(uint32_t);

        if (child->kind == EM_VFS_DIR) {
            /* build a header for this directory and add it */
            em_vfs_memory_header_t* directory =
                em_vfs_memory_calculate(child);
            em_vfs_memory_add(result, directory);
            /* this off by one is design */
            result->size.records--;
            free(directory);
        }
    } ZEND_HASH_FOREACH_END();

    return result;
}

static uintptr_t em_vfs_memory_export(em_vfs_node_t* node, em_vfs_memory_header_t* calculator) {
    /* Pass two, write memory */
    calculator->size.consumed =
        sizeof(em_vfs_memory_header_t) +
            calculator->size.header +
                calculator->size.length;

    em_vfs_memory_header_t* result =
        pecalloc(1, calculator->size.consumed, 1);

    memcpy(result, calculator,  sizeof(em_vfs_memory_header_t));
    memcpy(result->magic,       ZEND_STRL(EM_VFS_MEMORY_MAGIC));
    memcpy(result->version,     ZEND_STRL(EM_VFS_MEMORY_VERSION));

    uint32_t *offsets =
        (uint32_t*)
            (((char*) result) +
                sizeof(em_vfs_memory_header_t));

    em_vfs_memory_entry_t* entry =
        (em_vfs_memory_entry_t*)
            (((char*) result) +
                sizeof(em_vfs_memory_header_t) +
                    calculator->size.header);
    em_vfs_memory_entry_t* base = entry;

    em_vfs_memory_data(
        &entry, node, offsets, base);

    return (uintptr_t) result;
}

/**
 *  Shall allocate a contiguous buffer starting with em_vfs_stream_header_t
 *  The address is returned, and must be cast/unpacked by the caller
 *  It is the callers responsibliity to free the stream
 */
uintptr_t EMSCRIPTEN_KEEPALIVE em_vfs_memory_alloc(const char* path) {
    em_vfs_path_t* vpath =
        em_vfs_mkpath(path ? path : "/");
    em_vfs_node_t* node =
        em_vfs_resolve(vpath, false);
    em_vfs_path_release(vpath);
    em_vfs_memory_header_t* calculator =
        em_vfs_memory_calculate(node);
    uintptr_t memory = (uintptr_t)
        em_vfs_memory_export(node, calculator);
    free(calculator);
    return memory;
}

/**
 * Shall free a previously saved memory resource
 */
void EMSCRIPTEN_KEEPALIVE em_vfs_memory_free(void* memory) {
    free(memory);
}

/**
 * Shall parse version from string return indicator of success
 */
bool em_vfs_memory_version(em_vfs_memory_version_t* versioned, const char* version) {
    if (sscanf(version,
            "%d.%d.%d",
            &versioned->major,
            &versioned->minor,
            &versioned->patch) != 3) {
        return false;
    }

    versioned->combined =
        (versioned->major << 16) |
        (versioned->minor << 8)  |
        (versioned->patch << 0);

    return true;
}

/**
 * Check a given version may be loaded in this runtime
 * Note: for now just refuse to load anything from a later runtime
 */
bool em_vfs_memory_version_allowed(const char* version, char* error, size_t elength) {
    em_vfs_memory_version_t runtime;
    em_vfs_memory_version(
        &runtime,
        EM_VFS_MEMORY_VERSION);

    em_vfs_memory_version_t memory;
    if (!em_vfs_memory_version(&memory,  version)) {
        snprintf(error, elength,
            "the disk provided has a version (v%s) that cannot be parsed, "
            "it may be corrupt",
            version);
        return false;
    }

    if (runtime.combined < memory.combined) {
        snprintf(error, elength,
            "the disk provided was created in a more recent runtime "
            "(v%s) and cannot be loaded in the current runtime (v%s), "
            "please upgrade your runtime to use the disk",
            version, EM_VFS_MEMORY_VERSION);
        return false;
    }

    if (memory.major > runtime.major) {
        snprintf(error, elength,
            "the disk provided was created in v%d, "
            "the current runtime v%d is not able to load it",
            memory.major, runtime.major);
        return false;
    }

    if (memory.minor > runtime.minor) {
        snprintf(error, elength,
            "the disk provided was created in v%d.%d, "
            "the current runtime v%d.%d is not able to load it",
            memory.major, memory.minor,
            runtime.major, runtime.minor);
        return false;
    }

    return true;
}

/**
 * Shall load the memory pointed into the vfs
 * Shall start at offset-nth record (if given) and continue for records (if given)
 * Start at 0, for 0 records to load everything
 * Returns number of records written
 */
size_t EMSCRIPTEN_KEEPALIVE em_vfs_memory_write(void *memory, size_t offset, size_t records) {
    em_vfs_memory_header_t* header =
        (em_vfs_memory_header_t*) memory;
    if (!header || header->size.records == 0) {
        return 0;
    }

    char verror[1024];
    if (!em_vfs_memory_version_allowed(
            (char*) header->version,
            verror, sizeof(verror))) {
        fprintf(stderr,
            "[memory] %s", verror);
        return 0;
    }

    uint32_t total = header->size.records;
    uint32_t start = (offset < total) ? offset : 0;
    uint32_t end =
        (records == 0 || start + records > total) ? 
            total : start + records;

    uint32_t* offsets = 
        (uint32_t*)((char*)header +
            sizeof(em_vfs_memory_header_t));

    em_vfs_memory_entry_t* base =
        (em_vfs_memory_entry_t*)
            (((char*)header) +
                (sizeof(em_vfs_memory_header_t) +
                header->size.header));

#ifdef HAVE_EM_ZLIB
    size_t decompression = 0;
#endif

    for (uint32_t i = start; i < end; ++i) {
        em_vfs_memory_entry_t* entry =
            (em_vfs_memory_entry_t*)
                (((char*)base) + offsets[i]);

        char* name =
            (char*)(entry + 1);
        void* data =
            name + entry->size.name;

#ifndef HAVE_EM_ZLIB
        if (entry->flags & EM_VFS_MEMORY_COMPRESSED) {
            fprintf(stderr,
                "[memory] %s cannot decompress, no zlib support\n",
                name);
            continue;
        }
#endif

        if (entry->kind == EM_VFS_DIR) {
            if (em_vfs_mkdir(name)) {
                /**
                 * Yield to the browser so it can keep the ui snappy ...
                 */
                if ((records % 64) == 0) {
                    emscripten_sleep(10);
                }
                records++;
            }
        } else if (entry->kind == EM_VFS_FILE) {
            if (entry->flags & EM_VFS_MEMORY_COMPRESSED) {
#ifdef HAVE_EM_ZLIB
                void* copy = malloc(entry->size.data.verbatim);
                if (!copy) {
                    fprintf(stderr,
                        "[memory] %s failed to allocate for decompression\n",
                        name);
                    continue;
                }

                int zrc = uncompress(
                    (Bytef*) copy,
                    (uLongf *)
                        &entry->size.data.verbatim,
                    data,
                    entry->size.data.compressed);

                if (zrc != Z_OK) {
                    fprintf(stderr,
                        "[memory] %s failed to decompress, zrc=%d\n",
                        name, zrc);
                    free(copy);
                    continue;
                }

                if (em_vfs_put(name,
                        copy,
                        entry->size.data.verbatim)) {
                    /**
                     * Yield to the browser so it can keep the ui snappy ...
                     */
                    if (((decompression += 
                            entry->size.data.compressed) % 1024) == 0) {
                        emscripten_sleep(1);
                    }
                    records++;
                }

                free(copy);
                continue;
#endif
            }

            if (em_vfs_put(name,
                    (const char*)data,
                    entry->size.data.verbatim)) {
                /**
                 * Yield to the browser so it can keep the ui snappy ...
                 */
                if ((records % 64) == 0) {
                    emscripten_sleep(10);
                }
                records++;
            }
        }
    }

    return records;
}