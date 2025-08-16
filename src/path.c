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

char* em_vfs_path_normalize(const char *path) {
    // Strip vfs:// prefix if present
    const char *input =
        (strncmp(path, "vfs://", 6) == 0) ?
            path + 6 : path;

    // Duplicate input so we can tokenize safely
    char *work = pestrdup(input, 1);
    char *segments[256];
    int count = 0;
    bool has_trailing_slash =
        (input[strlen(input) - 1] == '/');

    // Tokenize and process segments
    char *token = strtok(work, "/");
    while (token) {
        if (strcmp(token, ".") == 0) {
            // skip
        } else if (strcmp(token, "..") == 0) {
            if (count > 0) count--; // pop last segment
        } else {
            segments[count++] = token;
        }
        token = strtok(NULL, "/");
    }

    // Allocate output buffer 
    size_t length = strlen(input) + 2;
    char *normalized = pecalloc(sizeof(char), length, 1);

    // Rebuild normalized path
    if (count == 0) {
        strcpy(normalized, "/");
    } else {
        for (int i = 0; i < count; i++) {
            strcat(normalized, "/");
            strcat(normalized, segments[i]);
        }
        // Preserve trailing slash if original had one
        if (has_trailing_slash) {
            strcat(normalized, "/");
        }
    }

    pefree(work, 1);
    return normalized;
}

em_vfs_path_t* em_vfs_mkpath(const char* path) {
    if (!path) {
        return NULL;
    }

    em_vfs_path_t* vpath = pecalloc(1, sizeof(em_vfs_path_t), 1);

    // Store normalized path
    char* normalized =
        vpath->original =
            em_vfs_path_normalize(path);

    // Find last slash
    char* last_slash = strrchr(normalized, '/');

    if (last_slash) {
        // Path has directory: "dir/file.txt"
        *last_slash = '\0';
        vpath->directory = pestrdup(normalized, 1);
        vpath->filename = pestrdup(last_slash + 1, 1);
        vpath->is_root = (strlen(vpath->directory) == 0);
    } else {
        // Path is just filename: "file.txt"
        vpath->directory = pestrdup("", 1);
        vpath->filename = pestrdup(normalized, 1);
        vpath->is_root = true;
    }

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