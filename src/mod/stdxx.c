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

#include <SAPI.h>

#include <srv/dispatch.h>
#include <vfs/vfs.h>

extern int em_stdio_map[1024];

static php_stream_ops           __php_stdio_ops__;
static php_stream_wrapper       __php_stdio_wrapper__;

typedef struct _php_stdio_stream_abstract_t {
    FILE* file;
    int   fd;
} php_stdio_stream_abstract_t;

ssize_t em_stdxx_write(php_stream* stream, const char* buffer, size_t length) {
    php_stdio_stream_abstract_t* abstract =
        (php_stdio_stream_abstract_t*)
            stream->abstract;
 
    /**
     * Redirect stdout/stderr to body buffer during requests
     */
    if ((em_stdio_map[abstract->fd] == fileno(stdout)) ||
        (em_stdio_map[abstract->fd] == fileno(stderr))) {
        em_dispatch_context_t* context = SG(server_context);

        if (!context) {
            goto __em_stdxx_write_fallback;
        }

        return (ssize_t) em_dispatch_response(
            context, EM_DISPATCH_BODY, buffer, length);
    }

__em_stdxx_write_fallback:
    return __php_stdio_ops__.write(stream, buffer, length);
}

ssize_t em_stdxx_read(php_stream* stream, char* buffer, size_t count) {
    return __php_stdio_ops__.read(stream, buffer, count);
}

int em_stdxx_close(php_stream* stream, int closefd) {
    return __php_stdio_ops__.close(stream, closefd);
}

int em_stdxx_flush(php_stream* stream) {
    return __php_stdio_ops__.flush(stream);
}

int em_stdxx_seek(php_stream *stream, zend_off_t offset, int whence, zend_off_t *newoffset) {
    return __php_stdio_ops__.seek(stream, offset, whence, newoffset);
}

int em_stdxx_cast(php_stream *stream, int castas, void **ret) {
    return __php_stdio_ops__.cast(stream, castas, ret);
}

int em_stdxx_option(php_stream *stream, int option, int value, void *ptrparam) {
    return __php_stdio_ops__.set_option(stream, option, value, ptrparam);
}

static php_stream_ops em_stdxx_ops = (php_stream_ops) {
    .write      = em_stdxx_write,
    .read       = em_stdxx_read,
    .close      = em_stdxx_close,
    .flush      = em_stdxx_flush,
    .label      = "em-stdio",
    .seek       = em_stdxx_seek,
    .cast       = em_stdxx_cast,
    .set_option = em_stdxx_option
};

void em_stdxx_startup(void) {
    memcpy(
        &__php_stdio_ops__,
        &php_stream_stdio_ops, 
        sizeof(php_stream_ops));
    memcpy(&__php_stdio_wrapper__,
        &php_plain_files_wrapper,
        sizeof(php_stream_wrapper));

    /**
     * Use internal stdio trampolines
     */
    memcpy(
        &php_stream_stdio_ops,
        &em_stdxx_ops,
        sizeof(php_stream_ops));
    /**
     * Use the vfs for plain files
     */
    memcpy(
        &php_plain_files_wrapper,
        &em_vfs_wrapper,
        sizeof(php_stream_wrapper));
}

void em_stdxx_shutdown(void) {
    /**
     * Put it all back the way it was ...
     */
    memcpy(
        &php_stream_stdio_ops,
        &__php_stdio_ops__,
        sizeof(php_stream_ops));
    memcpy(&php_plain_files_wrapper,
        &__php_stdio_wrapper__,
        sizeof(php_stream_wrapper));
}