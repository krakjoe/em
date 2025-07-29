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

#ifdef ZTS
#include <TSRM.h>
#endif

#ifdef EMSCRIPTEN
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include <SAPI.h>

#include <php_main.h>
#include <zend_exceptions.h>

static php_stream_wrapper em_http_wrapper;
static php_stream_ops     em_http_ops;

typedef struct _em_http_abstract_t {
    char*               data;
    ssize_t             length;
    size_t              position;
} em_http_abstract_t;

void* EMSCRIPTEN_KEEPALIVE em_http_buffer(uintptr_t ab, size_t length) {
    em_http_abstract_t* abstract =
        (em_http_abstract_t*) ab;

    abstract->data     = pecalloc(sizeof(char), length, 1);
    abstract->length   = length;
    abstract->position = 0;

    return abstract->data;
}

EM_JS(ssize_t, em_http_fetch, (const char* url, uintptr_t abstract), {
    var url_str = UTF8ToString(url);
    var xhr = new XMLHttpRequest();

    if (Module.dispatchEvent) {
        Module.dispatchEvent(new CustomEvent('io.begin', { 
            "detail": { 
                "url": url_str,
                "xhr": xhr}
        }));
    }

    try {
        xhr.open('GET', url_str, false);
        xhr.send();

        if (xhr.status >= 200 && xhr.status < 300) {
            var response = xhr.responseText;
            
            var length = Module.iou.getByteLength(response);
            var buffer = Module._em_http_buffer(abstract, length);

            length = Module.iou.toBytes(response, buffer);

            if (Module.dispatchEvent) {
                Module.dispatchEvent(new CustomEvent('io.end', { 
                    "detail": { 
                        "url":    url_str,
                        "xhr":    xhr,
                        "buffer": buffer,
                        "length": length}
                }));
            }

            return length;
        } else {
            if (Module.dispatchEvent) {
                Module.dispatchEvent(new CustomEvent('io.error', { 
                    "detail": { 
                        "url": url_str,
                        "xhr": xhr}
                }));
            }
        }
    } catch (exception) {
        if (Module.dispatchEvent) {
            Module.dispatchEvent(new CustomEvent('io.exception', {
                "detail": { 
                    "url":       url_str,
                    "xhr":       xhr,
                    "exception": exception,
                }
            }));
        }

        console.error('Fetch failed:', exception);
    }
    return -1;
});

static ssize_t em_http_stream_read(php_stream *stream, char *buffer, size_t count) {
    em_http_abstract_t* abstract =
        (em_http_abstract_t*)
            stream->abstract;

    // Nothing to read, return failure signal
    if (abstract->length <= 0) {
        return abstract->length;
    }

    // Nothing left to read, return EOF
    if (abstract->position == abstract->length) {
        return EOF;
    }

    /// Too much reading
    if (count > abstract->length - abstract->position) {
        count = abstract->length - abstract->position;
    }

    memcpy(buffer,
        &abstract->data[abstract->position],
        count);

    abstract->position += count;

    return count;
}

static void em_http_abstract_release(em_http_abstract_t* abstract) {
    if (!abstract) {
        return;
    }

    if (abstract->data) {
        pefree(abstract->data, 1);
    }

    pefree(abstract, 1);
}

static int em_http_stream_close(php_stream *stream, int type) {
    em_http_abstract_t* abstract =
        (em_http_abstract_t*) stream->abstract;

    // We never used this, points at garbage
    stream->orig_path = NULL;

    em_http_abstract_release(abstract);

    return SUCCESS;
}

static php_stream_ops em_http_ops = {
    NULL,                 // write
    em_http_stream_read,  // read  
    em_http_stream_close, // close
    NULL,                 // flush
    "em-http",
    NULL,                 // seek
    NULL,                 // cast
    NULL,                 // stat
    NULL                  // set_option
};

static php_stream *em_http_wrapper_open(php_stream_wrapper *wrapper, 
                                  const char *path, const char *mode,
                                  int options, zend_string **opened_path,
                                  php_stream_context *context) {
    em_http_abstract_t* abstract = 
        (em_http_abstract_t*)
            pecalloc(1, sizeof(em_http_abstract_t), 1);

    if ((abstract->length = em_http_fetch(
            path, (uintptr_t) abstract)) < 0) {
        em_http_abstract_release(abstract);
        return NULL; 
    }

    return php_stream_alloc(&em_http_ops, abstract, 0, mode);
}

static php_stream_wrapper_ops em_http_wrapper_ops = {
    em_http_wrapper_open,
    NULL, // close
    NULL, // fstat
    NULL, // stat
    NULL, // opendir
    "em-http"
};

static php_stream_wrapper em_http_wrapper = {
    &em_http_wrapper_ops,
    NULL,
    0
};

/* {{{ life cycle .. */
void em_http_startup(void) {}
void em_http_activate(void) {
    php_unregister_url_stream_wrapper("http");
    php_unregister_url_stream_wrapper("https");

    php_register_url_stream_wrapper("http",  &em_http_wrapper);
    php_register_url_stream_wrapper("https", &em_http_wrapper);
}
void em_http_deactivate(void) {
    php_unregister_url_stream_wrapper("http");
    php_unregister_url_stream_wrapper("https");
}
void em_http_shutdown(void) {} /* }}} */