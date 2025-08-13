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

#ifdef HAVE_EM_ZLIB
#include <zlib.h>
#endif

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
    (*entry)->kind = node->kind;
    (*entry)->flags      = 0;
    (*entry)->reserved   = 0;
    (*entry)->size.entry = sizeof(em_vfs_memory_entry_t);
    (*entry)->size.name  = em_vfs_memory_path_length(node);
    (*entry)->size.data  =
        (node->kind == EM_VFS_FILE) ?
            node->data.file.size : 0;
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
        memcpy(data,
            node->data.file.content,
            node->data.file.size);
        // Increase entry size by file size
        (*entry)->size.entry += node->data.file.size;
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

        ZEND_HASH_FOREACH_STR_KEY_PTR(
            &node->data.dir.children, name, child) {
            size_t cwritten =
                em_vfs_memory_data(
                    entry, child, &offsets[coffset], start);
            written += cwritten;
            coffset += cwritten;
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
            (em_vfs_memory_path_length(child)) + /* null terminated name */
            ((child->kind == EM_VFS_FILE) ?      /* size of the file */
                child->data.file.size : 0); 

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
    memcpy(result->magic,       ZEND_STRL("EMFS1\0"));
    memcpy(result->version,     ZEND_STRL("0.0.1\0"));

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
 * Shall load the memory pointed into the vfs
 * Shall start at offset-nth record (if given) and continue for records (if given)
 * Start at 0, for 0 records to load everything
 * Returns number of records written
 */
size_t EMSCRIPTEN_KEEPALIVE em_vfs_memory_write(void *memory, size_t offset, size_t records) {
    em_vfs_memory_header_t* header =
        (em_vfs_memory_header_t*) memory;
    if (!header ||
        header->size.records == 0) {
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

    for (uint32_t i = start; i < end; ++i) {
        em_vfs_memory_entry_t* entry =
            (em_vfs_memory_entry_t*)
                (((char*)base) + offsets[i]);

        char* name =
            (char*)(entry + 1);
        void* data =
            name + entry->size.name;

        if (entry->kind == EM_VFS_DIR) {
            if (em_vfs_mkdir(name)) {
                records++;
            }
        } else if (entry->kind == EM_VFS_FILE) {
            if (em_vfs_put(name, (const char*)data, entry->size.data)) {
                records++;
            }
        }
    }

    return records;
}