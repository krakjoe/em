
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

#ifndef HAVE_EM_MEMORY
#define HAVE_EM_MEMORY

#include <emscripten.h>

#include <php.h>

typedef struct _em_vfs_memory_header_t {
    uint8_t  magic[6];      /* EMFS1\0 */
    uint8_t  version[6];    /* Versioning Field */
    struct {
        uint32_t header;  /* Size of the header */
        uint32_t length;  /* Length of the stream */
        uint32_t records; /* Number of records in stream */
        uint32_t consumed;/* Total bytes consumed */
    } size;
    /* uint32_t* offsets; */
} __attribute__((__packed__)) em_vfs_memory_header_t;

typedef struct _em_vfs_memory_entry_t {
    uint8_t      kind;     /* The kind for this entry */
    uint8_t      flags;    /* Reserved */
    uint16_t     reserved; /* More reserved space */
    struct {
        uint32_t entry; /* The size of this entry */
        uint32_t name;  /* The size of the name of this entry */
        uint32_t data;  /* The size of the data following this entry */
    } size;
    struct {
        uint64_t ctime; /* The time this entry was created */
        uint64_t mtime; /* The time this entry was last modified */
    } stat;
    /* char name[size.name]; */
    /* char data[size.data]  */
} __attribute__((__packed__)) em_vfs_memory_entry_t;

/*
  Layout:

    [header]
    [offsets] - offsets describe the offset from entries address for each entry
    [entries]

  Entry:

    [entry]
    [name] entry.size.name bytes, null terminated
    [data] file data entry.size.data bytes
*/

/**
 *  Shall allocate a contiguous buffer starting with em_vfs_stream_header_t
 *  The address is returned, and must be cast/unpacked by the caller
 *  It is the callers responsibliity to free the stream
 *  Path is assumed to be root if null
 */
uintptr_t EMSCRIPTEN_KEEPALIVE em_vfs_memory_alloc(const char* path);
/**
 * Shall free a previously allocated stream resource
 */
void EMSCRIPTEN_KEEPALIVE em_vfs_memory_free(void* memory);
/**
 * Shall write the memory to the vfs
 * Shall start at offset-nth record (if given) and continue for records (if given)
 * Start at 0, for 0 records to load the entire stream
 */
size_t EMSCRIPTEN_KEEPALIVE em_vfs_memory_write(void *memory, size_t offset, size_t records);

#endif