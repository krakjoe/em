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
#include <vfs/vfs.h>
#include <vfs/node.h>

char   __em_cwd__[MAXPATHLEN] = {'/', '\0'};
size_t __em_cwd_size__        = 1;

int chdir(const char* path) {
    em_vfs_path_t* vpath =
        em_vfs_mkpath(path, true);
    em_vfs_node_t* node =
        em_vfs_resolve(vpath, false);

    if (!node) {
        errno =
            ENOENT;
        em_vfs_path_release(vpath);
        return FAILURE;
    }

    if (node->kind != EM_VFS_DIR) {
        errno =
            ENOTDIR;
        em_vfs_path_release(vpath);
        return FAILURE;
    }

    em_vfs_path_release(vpath);

    char* resolved =
        em_vfs_node_path(node);
    __em_cwd_size__ = strlen(resolved);
    memcpy(
        __em_cwd__,
        resolved,
        __em_cwd_size__);
    __em_cwd__[
        __em_cwd_size__] = 0;
    free(resolved);
    return SUCCESS;
}

char* getcwd(char* buf, size_t size) {
    if (__em_cwd_size__ > size) {
        errno =
            ERANGE;
        return NULL;
    }

    memcpy(
        buf,
        __em_cwd__,
        __em_cwd_size__);
    buf[__em_cwd_size__] = 0;
    return buf;
}

char* getwd(char* buf) {
    memcpy(
        buf,
        __em_cwd__,
        __em_cwd_size__);
    buf[__em_cwd_size__] = 0;
    return buf;
}
