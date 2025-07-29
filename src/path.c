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

#include "path.h"

em_vfs_path_t* em_vfs_mkpath(const char* path) {
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

void em_vfs_path_release(em_vfs_path_t* vpath) {
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