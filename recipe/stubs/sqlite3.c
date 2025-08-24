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
#include <emscripten.h>
#include <sqlite3.h>

#include <vfs/vfs.h>

/* SQLite file handle for em VFS */
typedef struct em_sqlite_file {
    sqlite3_file       base;      /* Base class. Must be first */
    em_vfs_abstract_t* abstract;  /* vfs abstract */
    int                flags;     /* Open flags */
} em_sqlite_file;

/* Forward declarations */
static int em_sqlite_close(sqlite3_file*);
static int em_sqlite_read(sqlite3_file*, void*, int iAmt, sqlite3_int64 iOfst);
static int em_sqlite_write(sqlite3_file*, const void*, int iAmt, sqlite3_int64 iOfst);
static int em_sqlite_truncate(sqlite3_file*, sqlite3_int64 size);
static int em_sqlite_sync(sqlite3_file*, int flags);
static int em_sqlite_file_size(sqlite3_file*, sqlite3_int64 *pSize);
static int em_sqlite_lock(sqlite3_file*, int);
static int em_sqlite_unlock(sqlite3_file*, int);
static int em_sqlite_check_reserved_lock(sqlite3_file*, int *pResOut);
static int em_sqlite_file_control(sqlite3_file*, int op, void *pArg);
static int em_sqlite_sector_size(sqlite3_file*);
static int em_sqlite_device_characteristics(sqlite3_file*);

static int em_sqlite_shm_map(sqlite3_file *pFile, int iPg, int pgsz, int bExtend, void volatile **pp);
static int em_sqlite_shm_lock(sqlite3_file *pFile, int offset, int n, int flags);
static void em_sqlite_shm_barrier(sqlite3_file *pFile);
static int em_sqlite_shm_unmap(sqlite3_file *pFile, int deleteFlag);
static int em_sqlite_shm_fetch(sqlite3_file *pFile, sqlite3_int64 iOfst, int iAmt, void **pp);
static int em_sqlite_shm_unfetch(sqlite3_file *pFile, sqlite3_int64 iOfst, void *pPage);

static int em_sqlite_open(sqlite3_vfs*, const char *zName, sqlite3_file*, int flags, int *pOutFlags);
static int em_sqlite_delete(sqlite3_vfs*, const char *zName, int syncDir);
static int em_sqlite_access(sqlite3_vfs*, const char *zName, int flags, int *pResOut);
static int em_sqlite_full_pathname(sqlite3_vfs*, const char *zName, int nOut, char *zOut);
static void* em_sqlite_dl_open(sqlite3_vfs*, const char *zFilename);
static void em_sqlite_dl_error(sqlite3_vfs*, int nByte, char *zErrMsg);
static void (*em_sqlite_dl_sym(sqlite3_vfs*, void*, const char *zSymbol))(void);
static void em_sqlite_dl_close(sqlite3_vfs*, void*);
static int em_sqlite_randomness(sqlite3_vfs*, int nByte, char *zOut);
static int em_sqlite_sleep(sqlite3_vfs*, int microseconds);
static int em_sqlite_current_time(sqlite3_vfs*, double*);
static int em_sqlite_current_time64(sqlite3_vfs *pVfs, sqlite3_int64 *);
static int em_sqlite_get_last_error(sqlite3_vfs*, int, char*);

/* File methods table */
static const sqlite3_io_methods em_sqlite_io_methods = {
    3,                                    /* iVersion */
    em_sqlite_close,                      /* xClose */
    em_sqlite_read,                       /* xRead */
    em_sqlite_write,                      /* xWrite */
    em_sqlite_truncate,                   /* xTruncate */
    em_sqlite_sync,                       /* xSync */
    em_sqlite_file_size,                  /* xFileSize */
    em_sqlite_lock,                       /* xLock */
    em_sqlite_unlock,                     /* xUnlock */
    em_sqlite_check_reserved_lock,        /* xCheckReservedLock */
    em_sqlite_file_control,               /* xFileControl */
    em_sqlite_sector_size,                /* xSectorSize */
    em_sqlite_device_characteristics,     /* xDeviceCharacteristics */
    em_sqlite_shm_map,
    em_sqlite_shm_lock,
    em_sqlite_shm_barrier,
    em_sqlite_shm_unmap,
    em_sqlite_shm_fetch,
    em_sqlite_shm_unfetch,
};

/* VFS structure */
static sqlite3_vfs em_sqlite_vfs = {
    2,                        /* iVersion */
    sizeof(em_sqlite_file),   /* szOsFile */
    1024,                     /* mxPathname */
    0,                        /* pNext */
    "em",                     /* zName */
    0,                        /* pAppData */
    em_sqlite_open,           /* xOpen */
    em_sqlite_delete,         /* xDelete */
    em_sqlite_access,         /* xAccess */
    em_sqlite_full_pathname,  /* xFullPathname */
    em_sqlite_dl_open,        /* xDlOpen */
    em_sqlite_dl_error,       /* xDlError */
    em_sqlite_dl_sym,         /* xDlSym */
    em_sqlite_dl_close,       /* xDlClose */
    em_sqlite_randomness,     /* xRandomness */
    em_sqlite_sleep,          /* xSleep */
    em_sqlite_current_time,   /* xCurrentTime */
    em_sqlite_get_last_error, /* xGetLastError */
    em_sqlite_current_time64  /* xCurrentTime64*/
};

char* em_sqlite_path(const char* zPath) {
    char* pPath = (char*) zPath;
    char* vresult;

    while (pPath[0] == '/')
        pPath++;

    if (strstr(pPath, "vfs:")) {
        pPath += sizeof("vfs:")-1;
    }

    em_vfs_path_t* vpath = em_vfs_mkpath(pPath);

    if (!vpath->directory && vpath->filename) {
        asprintf(&vresult,
            "/%s", vpath->filename);
    } else {
        asprintf(&vresult, 
            "/%s/%s", vpath->directory, vpath->filename);
    }

    em_vfs_path_release(vpath);
    return vresult;
}

static int em_sqlite_close(sqlite3_file* pFile) {
    em_sqlite_file* file =
        (em_sqlite_file*)pFile;
    em_vfs_close(file->abstract, false);
    em_vfs_release(file->abstract);
    return SQLITE_OK;
}

static int em_sqlite_read(sqlite3_file* pFile, void* zBuf, int iAmt, sqlite3_int64 iOfst) {
    em_sqlite_file* file = (em_sqlite_file*)pFile;

    if (iAmt == 0) {
        return SQLITE_OK;
    }

    ssize_t n = em_vfs_read_offset(file->abstract, zBuf, (size_t)iAmt, (size_t)iOfst);
    if (n < 0) {
        return SQLITE_IOERR_READ;
    }
    if (n < iAmt) {
        memset((char*)zBuf + n, 0, iAmt - n);
        // memvfs.c returns SQLITE_OK for short reads, zero-filling the rest
        return SQLITE_OK;
    }
    return SQLITE_OK;
}

static int em_sqlite_write(sqlite3_file* pFile, const void* zBuf, int iAmt, sqlite3_int64 iOfst) {
    em_sqlite_file* file = 
        (em_sqlite_file*)pFile;
    ssize_t n = em_vfs_write_offset(
        file->abstract, zBuf, (size_t)iAmt, (size_t)iOfst);
    if (n < 0 || n < iAmt) {
        return SQLITE_IOERR_WRITE;
    }
    return SQLITE_OK;
}

static int em_sqlite_truncate(sqlite3_file* pFile, sqlite3_int64 size) {
    em_sqlite_file* file =
        (em_sqlite_file*)pFile;
    em_vfs_truncate(file->abstract, (size_t) size);
    return SQLITE_OK;
}

static int em_sqlite_sync(sqlite3_file* pFile, int flags) {
    em_sqlite_file* file =
        (em_sqlite_file*)pFile;
    em_vfs_close(
        file->abstract, true);
    return SQLITE_OK;
}

static int em_sqlite_file_size(sqlite3_file* pFile, sqlite3_int64* pSize) {
    em_sqlite_file* file = (em_sqlite_file*)pFile;

    *pSize = (size_t)
        file->abstract->length;

    return SQLITE_OK;
}

static int em_sqlite_lock(sqlite3_file* pFile, int eLock) {
    /* No-op - single threaded environment */
    return SQLITE_OK;
}

static int em_sqlite_unlock(sqlite3_file* pFile, int eLock) {
    /* No-op - single threaded environment */
    return SQLITE_OK;
}

static int em_sqlite_check_reserved_lock(sqlite3_file* pFile, int* pResOut) {
    /* No-op - single threaded environment */
    *pResOut = 0;
    return SQLITE_OK;
}

static int em_sqlite_file_control(sqlite3_file* pFile, int op, void* pArg) {
    em_sqlite_file* file = (em_sqlite_file*) pFile;
    int rc = SQLITE_NOTFOUND;

    switch (op) {
        case SQLITE_FCNTL_VFSNAME:
            *(char**)pArg = sqlite3_mprintf("em");
            rc = SQLITE_OK;
            break;
        case 1: /* SQLITE_FCNTL_LOCKSTATE */
        case 10: /* SQLITE_FCNTL_PERSIST_WAL */
        case 13: /* SQLITE_FCNTL_POWERSAFE_OVERWRITE */
        case 15: /* SQLITE_FCNTL_PRAGMA */
        case 16: /* SQLITE_FCNTL_BUSYHANDLER */
        case 30: /* SQLITE_FCNTL_OVERWRITE */
            rc = SQLITE_OK;
            break;
        default:
            rc = SQLITE_NOTFOUND;
            break;
    }

    return rc;
}

static int em_sqlite_sector_size(sqlite3_file* pFile) {
    return 512;
}

static int em_sqlite_device_characteristics(sqlite3_file* pFile) {
    return SQLITE_IOCAP_SAFE_APPEND | 
           SQLITE_IOCAP_SEQUENTIAL;
}

static int em_sqlite_shm_map(
  sqlite3_file *pFile,
  int iPg,
  int pgsz,
  int bExtend,
  void volatile **pp
) {
  return SQLITE_IOERR_SHMMAP;
}

static int em_sqlite_shm_lock(sqlite3_file *pFile, int offset, int n, int flags){
  return SQLITE_IOERR_SHMLOCK;
}

static void em_sqlite_shm_barrier(sqlite3_file *pFile){
  return;
}

static int em_sqlite_shm_unmap(sqlite3_file *pFile, int deleteFlag){
  return SQLITE_OK;
}

static int em_sqlite_shm_fetch(
  sqlite3_file *pFile,
  sqlite3_int64 iOfst,
  int iAmt,
  void **pp
){
  em_sqlite_file *file =
    (em_sqlite_file *)pFile;
  *pp = (void*)(
    file->abstract->data +
        iOfst);
  return SQLITE_OK;
}

static int em_sqlite_shm_unfetch(sqlite3_file *pFile, sqlite3_int64 iOfst, void *pPage){
  return SQLITE_OK;
}

static int em_sqlite_open(sqlite3_vfs* pVfs, const char* zName, sqlite3_file* pFile,
                          int flags, int* pOutFlags) {
    char *path;
    em_sqlite_file* file =
        (em_sqlite_file*)pFile;

    memset(file, 0, sizeof(*file));

    file->base.pMethods = &em_sqlite_io_methods;
    file->flags = flags;

    if (!zName) {
        static int temp_counter = 0;
        char temp_name[64];
        snprintf(temp_name, sizeof(temp_name),
                 "temp_%lld_%d.db", (long long)time(NULL), ++temp_counter);
        path = strdup(temp_name);
    } else {
        path = em_sqlite_path(zName);
    }

    if (!path) {
        return SQLITE_NOMEM;
    }

    char mode[4] = "";
    if (flags & SQLITE_OPEN_READWRITE) {
        strcat(mode, "rw");
    } else if (flags & SQLITE_OPEN_READONLY) {
        strcat(mode, "r");
    }
    if (flags & SQLITE_OPEN_CREATE) {
        if (!strchr(mode, 'w')) strcat(mode, "w");
    }
    if (mode[0] == '\0') {
        strcpy(mode, "r");
    }

    em_vfs_abstract_t* abstract = em_vfs_open(path, mode);

    if (!abstract) {
        free(path);
        return SQLITE_CANTOPEN;
    }

    file->abstract = abstract;

    if (pOutFlags) {
        *pOutFlags = flags;
    }

    free(path);
    return SQLITE_OK;
}

static int em_sqlite_delete(sqlite3_vfs* pVfs, const char* zName, int syncDir) {
    char* vpath =
        em_sqlite_path(zName);
    bool result =
        em_vfs_unlink(vpath, false);
    free(vpath);

    return result ? SQLITE_OK : SQLITE_IOERR_DELETE;
}

static int em_sqlite_access(sqlite3_vfs* pVfs, const char* zName, int flags, int* pResOut) {
    if (flags == SQLITE_ACCESS_READWRITE) {
        *pResOut = 1;
        return SQLITE_OK;
    }

    if (flags == SQLITE_ACCESS_EXISTS) {
        char* path = em_sqlite_path(zName);
        em_vfs_abstract_t* abstract = em_vfs_open(path, "r");
        if (abstract) {
            *pResOut = 1;
            em_vfs_release(abstract);
        } else {
            *pResOut = 0;
        }
        free(path);
        return SQLITE_OK;
    }

    *pResOut = 0;
    return SQLITE_OK;
}

static int em_sqlite_full_pathname(
    sqlite3_vfs* pVfs,
    const char* zName,
    int nOut,
    char* zOut
) {
    char* vpath = em_sqlite_path(zName);
    if (!vpath) {
        return SQLITE_NOMEM;
    }

    size_t len = strlen(vpath);
    if (nOut <= 0) {
        free(vpath);
        return SQLITE_OK;
    }

    if (len + 1 > (size_t)nOut) {
        /* Truncate, but ensure null termination within bounds */
        memcpy(zOut, vpath, nOut - 1);
        zOut[nOut - 1] = '\0';
    } else {
        /* Copy including the null terminator */
        memcpy(zOut, vpath, len + 1);
    }

    free(vpath);
    return SQLITE_OK;
}

/* Stub implementations for dynamic loading (not supported) */
static void* em_sqlite_dl_open(sqlite3_vfs* pVfs, const char* zFilename) {
    return NULL;
}

static void em_sqlite_dl_error(sqlite3_vfs* pVfs, int nByte, char* zErrMsg) {
    snprintf(zErrMsg, nByte, "Dynamic loading not supported");
}

static void (*em_sqlite_dl_sym(sqlite3_vfs* pVfs, void* pHandle, const char* zSymbol))(void) {
    return NULL;
}

static void em_sqlite_dl_close(sqlite3_vfs* pVfs, void* pHandle) {
    /* No-op */
}

#define DELEGATE(p) ((sqlite3_vfs*)((p)->pAppData))

static int em_sqlite_randomness(sqlite3_vfs* pVfs, int nByte, char* zOut) {
    return DELEGATE(pVfs)->xRandomness(DELEGATE(pVfs), nByte, zOut);
}

static int em_sqlite_sleep(sqlite3_vfs* pVfs, int microseconds) {
    /* No-op in single threaded environment */
    return SQLITE_OK;
}

static int em_sqlite_current_time(sqlite3_vfs* pVfs, double* prNow) {
    return DELEGATE(pVfs)->xCurrentTime(DELEGATE(pVfs), prNow);
}

static int em_sqlite_get_last_error(sqlite3_vfs* pVfs, int nBuf, char* zBuf) {
    return DELEGATE(pVfs)->xGetLastError(DELEGATE(pVfs), nBuf, zBuf);
}

static int em_sqlite_current_time64(sqlite3_vfs *pVfs, sqlite3_int64 *prNow) {
  return DELEGATE(pVfs)->xCurrentTimeInt64(DELEGATE(pVfs), prNow);
}

#undef DELEGATE

/* Public interface */
void em_sqlite_vfs_register(void) {
    em_sqlite_vfs.pAppData =
        sqlite3_vfs_find(NULL);              /* underlying default VFS */
    sqlite3_vfs_register(&em_sqlite_vfs, 1); /* make em vfs the default */
}

void em_sqlite_vfs_unregister(void) {
    sqlite3_vfs_unregister(&em_sqlite_vfs);
}