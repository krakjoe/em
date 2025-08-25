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
#include <glob.h>
#include <vfs/vfs.h>
#include <vfs/iterator.h>

typedef int (*glob_error_function_t)
    (const char* epath, int eerrno);

static int em_glob_match(const char *pattern, const char *name) {
    while (*pattern && *name) {
        if (*pattern == '*') {
            pattern++;
            if (!*pattern) {
                return 1;
            }
            while (*name) {
                if (em_glob_match(pattern, name)) {
                    return 1;
                }
                name++;
            }
            return 0;
        } else if (*pattern == '?') {
            pattern++; name++;
        } else if (*pattern == *name) {
            pattern++; name++;
        } else {
            return 0;
        }
    }

    if (*pattern == '*' && !*(pattern+1)) {
        return 1;
    }

    return !*pattern && !*name;
}

int glob(const char* pattern, int flags, glob_error_function_t error, glob_t* buffer) {
    char* parse;
    bool directories = false;
    if (pattern[strlen(pattern) - 1] == '/') {
        parse = (char*) strndup(
            pattern, strlen(pattern)-1);
        directories = true;
    } else {
        parse = (char*) pattern;
    }

    em_vfs_path_t* vpath =
        em_vfs_mkpath(parse, false);

    fprintf(stderr, "vpath: %s / %s\n",
        vpath->directory, vpath->filename);

    em_vfs_iterator_t* it =
        em_vfs_iterator(vpath->directory);

    memset(buffer, 0, sizeof(glob_t));

    if (!it || !em_vfs_iterator_count(it)) {
        if (error) {
            error(vpath->directory, ENOENT);
        }
        goto __em_glob_leave;
    }

    buffer->gl_pathc = 0;
    buffer->gl_pathv = calloc(
        em_vfs_iterator_count(it) + 1,
        sizeof(char*));

    if (!buffer->gl_pathv) {
        if (error) {
            error(vpath->directory, ENOMEM);
        }
        goto __em_glob_leave;
    }

    do {
        const char* name = (const char*)
            em_vfs_iterator_name(it);

        if (em_glob_match(vpath->filename, name)) {
            char* path = calloc(
                strlen(vpath->directory) +
                strlen(name) + 2 +
                (((flags & GLOB_MARK) || directories ? 1 : 0)),
                sizeof(char));
            strcat(path, vpath->directory);
            if (path[strlen(path) - 1] != '/') {
                strcat(path, "/");
            }
            strcat(path, (name[0] == '/') ?
                &name[1] : &name[0]);
            if (((flags & GLOB_MARK) ||
                    (directories == true))) {
                strcat(path, "/");
            }

            path[strlen(path)] = 0;

            buffer->gl_pathv[
                buffer->gl_pathc++] = path;
        }
    } while (em_vfs_iterator_next(it));

    buffer->gl_pathv[buffer->gl_pathc] = NULL;

__em_glob_leave:
    if (it) {
        em_vfs_iterator_free(it);
    }
    em_vfs_path_release(vpath);
    if (directories) {
        free(parse);
    }
    return buffer->gl_pathc ?
        SUCCESS : GLOB_NOMATCH;
}

void globfree(glob_t* buffer) {
    if (!buffer) {
        return;
    }

    if (!buffer->gl_pathv) {
        return;
    }

    for (size_t idx = 0; idx < buffer->gl_pathc; ++idx) {
        free(buffer->gl_pathv[idx]);
    }

    free(buffer->gl_pathv);
}