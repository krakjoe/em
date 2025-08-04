/*
  zip_source_file_stdio_named.c -- source for stdio file opened by name
  Copyright (C) 1999-2023 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <info@libzip.org>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
  3. The names of the authors may not be used to endorse or promote
     products derived from this software without specific prior
     written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 libzip abstracts away streams for internal use, and they expose a source
 creation API where you may control the stream, however this is not useful
 unless you are the first order consumer of the libzip API.

 In the case of this project it is necessary to provide functioning libzip to
 ext/zip without modifying source code of the extension, there is no API to do
 that. Instead we have to patch the object after linking.

 This file was lifted from the libzip source and modified to work with em VFS.

 This is transparent to the ext/zip extension.
*/

#include <zip.h>
#include <php.h>

#include "../../src/vfs.h"

typedef struct zip_string zip_string_t;
typedef struct zip_entry zip_entry_t;
typedef struct zip_hash zip_hash_t;
typedef struct zip_progress zip_progress_t;

struct zip {
    zip_source_t *src;       /* data source for archive */
    unsigned int open_flags; /* flags passed to zip_open */
    zip_error_t error;       /* error information */

    unsigned int flags;    /* archive global flags */
    unsigned int ch_flags; /* changed archive global flags */

    char *default_password; /* password used when no other supplied */

    zip_string_t *comment_orig;    /* archive comment */
    zip_string_t *comment_changes; /* changed archive comment */
    bool comment_changed;          /* whether archive comment was changed */

    zip_uint64_t nentry;       /* number of entries */
    zip_uint64_t nentry_alloc; /* number of entries allocated */
    zip_entry_t *entry;        /* entries */

    zip_uint64_t nopen_source;       /* number of open sources using archive */
    zip_uint64_t nopen_source_alloc; /* number of sources allocated */
    zip_source_t **open_source;      /* open sources using archive */

    zip_hash_t *names; /* hash table for name lookup */

    zip_progress_t *progress; /* progress callback for zip_close() */

    zip_uint32_t* write_crc; /* have _zip_write() compute CRC */
    time_t torrent_mtime;
};

ZIP_EXTERN zip_source_t *
    zip_source_file_create(const char *fname, zip_uint64_t start, zip_int64_t length, zip_error_t *error);
ZIP_EXTERN zip_source_t *
    zip_source_file(zip_t *za, const char *fname, zip_uint64_t start, zip_int64_t len);

    /* We need these struct/type definitions from libzip internals */
typedef struct zip_source_file_context zip_source_file_context_t;
typedef struct zip_source_file_operations zip_source_file_operations_t;
typedef struct zip_source_file_stat zip_source_file_stat_t;

struct zip_source_file_context {
    zip_error_t error; /* last error information */
    zip_int64_t supports;

    /* reading */
    char *fname;                      /* name of file to read from */
    em_vfs_abstract_t *f;             /* file to read from */
    zip_stat_t st;                    /* stat information passed in */
    zip_file_attributes_t attributes; /* additional file attributes */
    zip_error_t stat_error;           /* error returned for stat */
    zip_uint64_t start;               /* start offset of data to read */
    zip_uint64_t len;                 /* length of the file, 0 for up to EOF */
    zip_uint64_t offset;              /* current offset relative to start (0 is beginning of part we read) */

    /* writing */
    char *tmpname;
    em_vfs_abstract_t *fout;

    zip_source_file_operations_t *ops;
    void *ops_userdata;
};

struct zip_source_file_operations {
    void (*close)(zip_source_file_context_t *ctx);
    zip_int64_t (*commit_write)(zip_source_file_context_t *ctx);
    zip_int64_t (*create_temp_output)(zip_source_file_context_t *ctx);
    zip_int64_t (*create_temp_output_cloning)(zip_source_file_context_t *ctx, zip_uint64_t len);
    bool (*open)(zip_source_file_context_t *ctx);
    zip_int64_t (*read)(zip_source_file_context_t *ctx, void *buf, zip_uint64_t len);
    zip_int64_t (*remove)(zip_source_file_context_t *ctx);
    void (*rollback_write)(zip_source_file_context_t *ctx);
    bool (*seek)(zip_source_file_context_t *ctx, void *f, zip_int64_t offset, int whence);
    bool (*stat)(zip_source_file_context_t *ctx, zip_source_file_stat_t *st);
    char *(*string_duplicate)(zip_source_file_context_t *ctx, const char *);
    zip_int64_t (*tell)(zip_source_file_context_t *ctx, void *f);
    zip_int64_t (*write)(zip_source_file_context_t *ctx, const void *data, zip_uint64_t len);
};

extern zip_source_t* zip_source_file_common_new(
    const char *fname, void *file, zip_uint64_t start, zip_int64_t len, const zip_stat_t *st, zip_source_file_operations_t *ops, void *ops_userdata, zip_error_t *error);

struct zip_source_file_stat {
    zip_uint64_t size; /* must be valid for regular files */
    time_t mtime;      /* must always be valid, is initialized to current time */
    bool exists;       /* must always be valid */
    bool regular_file; /* must always be valid */
};

/* Function declarations for our em VFS implementation */
static bool _em_zip_op_open(zip_source_file_context_t *ctx);
static zip_int64_t _em_zip_op_read(zip_source_file_context_t *ctx, void *buf, zip_uint64_t len);
static zip_int64_t _em_zip_op_write(zip_source_file_context_t *ctx, const void *data, zip_uint64_t len);
static bool _em_zip_op_seek(zip_source_file_context_t *ctx, void *f, zip_int64_t offset, int whence);
static zip_int64_t _em_zip_op_commit_write(zip_source_file_context_t *ctx);
static bool _em_zip_op_stat(zip_source_file_context_t *ctx, zip_source_file_stat_t *st);
static zip_int64_t _em_zip_op_tell(zip_source_file_context_t *ctx, void* f);
static zip_int64_t _em_zip_op_remove(zip_source_file_context_t *ctx);
static void _em_zip_op_rollback_write(zip_source_file_context_t *ctx);
static char *_em_zip_op_strdup(zip_source_file_context_t *ctx, const char *string);
static void _em_zip_op_close(zip_source_file_context_t *ctx);
static zip_int64_t _em_zip_op_create_temp_output(zip_source_file_context_t *ctx);

/* Our VFS operations table */
static zip_source_file_operations_t ops_em_vfs = {
    _em_zip_op_close,
    _em_zip_op_commit_write,
    _em_zip_op_create_temp_output,
    NULL, /* no cloning support needed */
    _em_zip_op_open,
    _em_zip_op_read,
    _em_zip_op_remove,
    _em_zip_op_rollback_write,
    _em_zip_op_seek,
    _em_zip_op_stat,
    _em_zip_op_strdup,
    _em_zip_op_tell,
    _em_zip_op_write
};

ZIP_EXTERN zip_source_t* zip_source_file(zip_t *za, const char *fname, zip_uint64_t start, zip_int64_t len) {
    if (za == NULL)
        return NULL;

    return zip_source_file_create(fname, start, len, &za->error);
}

ZIP_EXTERN zip_source_t* zip_source_file_create(const char *fname, zip_uint64_t start, zip_int64_t length, zip_error_t *error) {
    if (fname == NULL || length < -1) {
        zip_error_set(error, ZIP_ER_INVAL, 0);
        return NULL;
    }
    return zip_source_file_common_new(fname, NULL, start, length, NULL, &ops_em_vfs, NULL, error);
}

static bool _em_zip_op_open(zip_source_file_context_t *ctx) {
    ctx->f = em_vfs_open(ctx->fname, "r");
    if (!ctx->f) {
        zip_error_set(&ctx->error, ZIP_ER_OPEN, ENOENT);
        return false;
    }
    return true;
}

static zip_int64_t _em_zip_op_read(zip_source_file_context_t *ctx, void *buf, zip_uint64_t len) {
    return em_vfs_read(ctx->f, buf, len);
}

static zip_int64_t _em_zip_op_write(zip_source_file_context_t *ctx, const void *data, zip_uint64_t len) {
    return em_vfs_write(ctx->fout, data, len);
}

static bool _em_zip_op_seek(zip_source_file_context_t *ctx, void *f, zip_int64_t offset, int whence) {
    zend_off_t position;
    return em_vfs_seek(
        f, offset, whence, &position) == SUCCESS;
}

static zip_int64_t _em_zip_op_tell(zip_source_file_context_t *ctx, void* f) {
    em_vfs_abstract_t* abstract =
        (em_vfs_abstract_t*) f;
    return abstract->position;
}

static bool
_em_zip_op_stat(zip_source_file_context_t *ctx, zip_source_file_stat_t *st) {
    if (!ctx->fname) {
        // No filename = file doesn't exist yet
        st->exists = false;
        return true;
    }

    php_stream_statbuf ssb;
    em_vfs_path_t* vpath = em_vfs_mkpath(ctx->fname);

    if (em_vfs_stat_path(
            vpath, &ssb, true) == FAILURE) {
        st->exists = false;

        em_vfs_path_release(vpath);
        return true;
    }

    st->exists = true;
    st->size = ssb.sb.st_size;
    st->mtime = ssb.sb.st_mtime;
    st->regular_file = true;

    // Set Unix-compatible attributes like libzip does
    ctx->attributes.valid = ZIP_FILE_ATTRIBUTES_HOST_SYSTEM | 
                          ZIP_FILE_ATTRIBUTES_EXTERNAL_FILE_ATTRIBUTES;
    ctx->attributes.host_system = ZIP_OPSYS_UNIX;
    ctx->attributes.external_file_attributes = ((0666 << 16) | 1);
    em_vfs_path_release(vpath);

    return true;
}

static void _em_zip_op_close(zip_source_file_context_t *ctx) {
    if (ctx->f) {
        em_vfs_close(
            ctx->f, false);
        ctx->f = NULL;
    }
}

static zip_int64_t _em_zip_op_create_temp_output(zip_source_file_context_t *ctx) {
    char *temp;
    size_t templen = strlen(ctx->fname) + 8;
    
    temp = malloc(templen);
    if (!temp) {
        zip_error_set(&ctx->error, ZIP_ER_MEMORY, 0);
        return -1;
    }

    snprintf(temp, templen, "%s.XXXXXX", ctx->fname);
    ctx->tmpname = temp;

    ctx->fout = em_vfs_open(ctx->tmpname, "w");
    if (!ctx->fout) {
        free(temp);
        ctx->tmpname = NULL;
        zip_error_set(&ctx->error, ZIP_ER_TMPOPEN, 0);
        return -1;
    }
    
    return 0;
}

static zip_int64_t _em_zip_op_commit_write(zip_source_file_context_t *ctx) {
    if (ctx->fout) {
        em_vfs_close(ctx->fout, false);
        ctx->fout = NULL;
    }

    em_vfs_unlink(ctx->fname, false);

    if (!em_vfs_move(ctx->tmpname, ctx->fname)) {
        zip_error_set(&ctx->error, ZIP_ER_RENAME, 0);
        return -1;
    }

    return 0;
}

static zip_int64_t _em_zip_op_remove(zip_source_file_context_t *ctx) {
    return em_vfs_unlink(ctx->fname, false);
}

static void _em_zip_op_rollback_write(zip_source_file_context_t *ctx) {
    if (ctx->fout) {
        em_vfs_close(ctx->fout, false);
        ctx->fout = NULL;
    }
    if (ctx->tmpname) {
        em_vfs_unlink(ctx->tmpname, false);
    }
}

static char *_em_zip_op_strdup(zip_source_file_context_t *ctx, const char *string) {
    size_t len = strlen(string) + 1;
    char *copy = malloc(len);
    if (copy) {
        memcpy(copy, string, len);
    }
    return copy;
}