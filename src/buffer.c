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

#include "buffer.h"

em_buffer_t __em_response_buffer = EM_BUFFER_EMPTY;
em_buffer_t __em_request_buffer  = EM_BUFFER_EMPTY;

void em_buffer_clear(em_buffer_t* buffer, bool _free) {
    if (buffer->value) {
        if (_free) {
            free(buffer->value);

            buffer->value = NULL;
            buffer->max   = 0;
        } else {
            memset(
                buffer->value,
                0, buffer->max);
        }
    } else {
        buffer->max = 0;
    }
    buffer->length   = 0;
    buffer->position = 0;
}

size_t em_buffer_write(em_buffer_t* buffer, const char* buf, size_t len) {
    if (buffer->length + len >= buffer->max) {
        buffer->max = buffer->length + len + 1024;
        buffer->value = realloc(
            buffer->value, buffer->max);
        if (!buffer->value) {
            return 0;
        }
    }

    memcpy(
    buffer->value +
        buffer->length,
    buf, len);
    buffer->length += len;
    buffer->value[
        buffer->length] = 0;
    return len;
} 

size_t em_buffer_response(const char* buf, size_t len) {
    return em_buffer_write(&__em_response_buffer, buf, len);
}

size_t em_buffer_request(const char* buf, size_t len) {
    return em_buffer_write(&__em_request_buffer, buf, len);
}