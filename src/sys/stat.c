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

/**
 * Many calls within zend up in stat, so we take over here ...
 */
int stat(const char *restrict path, struct stat *restrict buf) {
    em_vfs_path_t* vpath =
        em_vfs_mkpath(path, false);

    php_stream_statbuf ssb;
    if (em_vfs_stat_path(
            vpath, &ssb, false) != SUCCESS) {
        em_vfs_path_release(vpath);
        return FAILURE;
    }

    memcpy(
        buf,
        &ssb.sb,
        sizeof(struct stat));
    em_vfs_path_release(vpath);
    return SUCCESS;
}