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

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#define IPC_PRIVATE 0
#define IPC_CREAT   01000
#define IPC_RMID    0
#define IPC_SET     1
#define IPC_STAT    2
#define SHM_R       0400
#define SHM_W       0200

#define EM_SHM_MAX_SEGMENTS 64

typedef enum _em_shm_selector_t {
    EM_SHM_ID      = 1,
    EM_SHM_KEY     = 2,
    EM_SHM_ADDRESS = 4,
} em_shm_selector_t;

typedef struct _em_shm_t {
    int id;
    int key;
    int flags;
    int refcount;
    bool deleted;
    struct {
        void*  address;
        size_t size;
    } segment;
} em_shm_t;

static em_shm_t __em_shm_segments__[EM_SHM_MAX_SEGMENTS];
static int      __em_shm_next__        = 1;
static bool     __em_shm_initialized__ = false;

void em_shm_startup(void) {
    if (__em_shm_initialized__) {
        return;
    }
    memset(
        &__em_shm_segments__,
        0,
        sizeof(__em_shm_segments__));
    __em_shm_initialized__ = true;
}

static em_shm_t* em_shm_alloc(int key, size_t size, int flags) {
    em_shm_t* search  = __em_shm_segments__;
    em_shm_t* end =
        search + EM_SHM_MAX_SEGMENTS;

    while (search < end) {
        if (!search->segment.address) {
            memset(search, 0, sizeof(em_shm_t));
            search->segment.address =
                calloc(sizeof(char), size);
            if (!search->segment.address) {
                errno =
                    ENOMEM;
                return NULL;
            }
            search->segment.size = size;
            search->flags = flags;
            search->key = key;
            search->id =
                __em_shm_next__++;
            return search;
        }
        search++;
    }

    errno = ENOSPC;

    return NULL;
}

static void em_shm_free(em_shm_t* selected) {
    if (selected->segment.address) {
        free(selected->segment.address);
    }
    memset(selected, 0, sizeof(em_shm_t));
}

void em_shm_shutdown(void) {
    em_shm_t* search  = __em_shm_segments__;
    em_shm_t* end =
        search + EM_SHM_MAX_SEGMENTS;

    while (search < end) {
        em_shm_free(search);
        search++;
    }

    __em_shm_initialized__ = false;
}

static em_shm_t* em_shm_find(em_shm_selector_t selector, intptr_t selected, size_t size, int flags) {
    if (selector == EM_SHM_KEY && selected == IPC_PRIVATE) {
        goto __em_shm_alloc;
    }

    em_shm_t* search  = __em_shm_segments__;
    em_shm_t* end =
        search + EM_SHM_MAX_SEGMENTS;

    while (search < end) {
        switch (selector) {
            case EM_SHM_ID:
                if (search->id == (int) selected) {
                    goto __em_shm_found;
                }
            break;

            case EM_SHM_KEY:
                if (search->key == (int) selected) {
                    goto __em_shm_found;
                }
            break;

            case EM_SHM_ADDRESS:
                if (search->segment.address == (void*) selected) {
                    goto __em_shm_found;
                }
            break;
        }
        search++;
    }

    if (size && flags & IPC_CREAT) {
__em_shm_alloc:
        return em_shm_alloc(
            (int) selected, size, flags);
    }

    {
        errno = ENOENT;
    }
__em_shm_null:
    return NULL;

__em_shm_found:
    if (size && size > search->segment.size) {
        errno =
            EINVAL;
        goto __em_shm_null;
    }

    return search;
}

int shmget(int key, size_t size, int flags) {
    em_shm_t* selected =
        em_shm_find(EM_SHM_KEY, key, size, flags);
    if (!selected) {
        return -1;
    }
    return selected->id;
}

void* shmat(int id, const void *address, int flags) {
    em_shm_t* selected = em_shm_find(EM_SHM_ID, id, 0, flags);

    if (!selected) {
        return (void*) -1;
    }

    selected->refcount++;

    return selected->segment.address;
}

int shmdt(const void *address) {
    em_shm_t* selected = em_shm_find(
        EM_SHM_ADDRESS, (intptr_t) address, 0, 0);
    if (!selected) {
        errno =
            EINVAL;
        return -1;
    }

    if (--selected->refcount == 0) {
        if (selected->deleted) {
            em_shm_free(selected);
        }
    }
    return 0; 
}

int shmctl(int id, int cmd, void *buffer) {
    em_shm_t* selected =
        em_shm_find(EM_SHM_ID, id, 0, 0);

    if (!selected) {
        errno =
            EINVAL;
        return -1;
    }

    switch (cmd) {
        case IPC_RMID:
            selected->deleted = true;

            if (selected->refcount == 0) {
                em_shm_free(selected);
            }
        return 0;

        case IPC_STAT:
            if (!buffer) {
                errno =
                    EINVAL;
                return -1;
            }
        return 0;

        case IPC_SET:
            if (!buffer) {
                errno =
                    EINVAL;
                return -1;
            }
        return 0;
    }

    errno =
        EINVAL;
    return -1; 
}