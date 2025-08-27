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

#include <vfs/path.h>

static char* __em_vfs_path_empty__ = NULL;
static char* __em_vfs_path_root__  = NULL;
 
void em_vfs_path_startup(void) {
    __em_vfs_path_empty__ = strdup("");
    __em_vfs_path_root__  = strdup("/");
}

static char* em_vfs_path_working(const char* path) {
    if (path[0] == '/') {
        return strdup(path);
    }

    char cwd[MAXPATHLEN];
    getcwd(
        cwd, MAXPATHLEN);
    size_t length = strlen(cwd);

    char* working = calloc(
        length  + strlen(path) + 1,
        sizeof(char));

    memcpy(
        working,
        cwd,
        length);
    memcpy(
        &working[length],
        path, strlen(path));
    return working;
}

static char* em_vfs_path_normalize(const char *path, bool directory) {
    // Strip vfs:// prefix if present
    const char *input =
        (strncmp(path, "vfs://", 6) == 0) ?
            path + 6 : path;

    // Duplicate input so we can tokenize safely
    char *work = em_vfs_path_working(input);
    size_t length = strlen(work);
    char *segments[256];
    int count = 0;
    bool has_trailing_slash =
        (work[length - 1] == '/');

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

    char *normalized = calloc(length + count + 2, sizeof(char));

    // Rebuild normalized path
    if (count == 0) {
        strcpy(normalized, "/");
    } else {
        for (int i = 0; i < count; i++) {
            strcat(normalized, "/");
            strcat(normalized, segments[i]);
        }
        // Preserve trailing slash if original had one
        if (directory || has_trailing_slash) {
            strcat(normalized, "/");
        }
    }

    free(work);
    return normalized;
}

em_vfs_path_t* em_vfs_mkpath(const char* path, bool directory) {
    if (!path) {
        return NULL;
    }

    em_vfs_path_t* vpath = calloc(1, sizeof(em_vfs_path_t));

    // Store normalized path
    char* normalized =
        vpath->original =
            em_vfs_path_normalize(path, directory);

    size_t length = strlen(normalized);

    /** Special case for sizeof(1) paths **/
    if (length == 1) {
        if (normalized[0] == '/') {
            vpath->directory = __em_vfs_path_root__;
            vpath->filename  = __em_vfs_path_empty__;
        } else {
            vpath->directory = __em_vfs_path_empty__;
            vpath->filename  = strdup(normalized);
        }
        return vpath;
    }

    // Find last slash
    char* last_slash = strrchr(normalized, '/');

    if (last_slash) {
        // Path has directory: "dir/file.txt"
        vpath->directory = strndup(
            normalized, last_slash - normalized);
        vpath->filename = strdup(last_slash + 1);
        vpath->is_root = (strlen(vpath->directory) == 0);
    } else {
        // Path is just filename: "file.txt"
        vpath->directory = __em_vfs_path_empty__;
        vpath->filename = strdup(normalized);
        vpath->is_root = true;
    }

    return vpath;
}

static void em_vfs_path_free(char* element) {
    if (!element) {
        return;
    }

    if (element == __em_vfs_path_root__) {
        return;
    }

    if (element == __em_vfs_path_empty__) {
        return;
    }

    free(element);
}

void em_vfs_path_release(em_vfs_path_t* vpath) {
    if (!vpath) {
        return;
    }

    em_vfs_path_free(vpath->original);
    em_vfs_path_free(vpath->directory);
    em_vfs_path_free(vpath->filename);

    free(vpath);
}

void em_vfs_path_shutdown(void) {
    free(__em_vfs_path_root__);
    free(__em_vfs_path_empty__);
}