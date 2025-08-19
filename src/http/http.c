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

#include "http.h" 
#include "request.h"

#include <php_network.h>

static ssize_t em_http_stream_read(php_stream *stream, char *buffer, size_t count) {
    em_http_abstract_t* abstract =
        (em_http_abstract_t*) stream->abstract;

    while (!(abstract->state & EM_HTTP_COMPLETE)) {
        // Yield control to browser
        emscripten_sleep(100);
        // Yield control to userland
        if (!abstract->options.blocking) {
            errno = EWOULDBLOCK;
            return -1;
        }
    }

    em_http_request_event_clear(abstract);

    if (abstract->state & EM_HTTP_TIMEOUT) {
        errno = ETIMEDOUT;
        return -1;
    }

    if (abstract->state & EM_HTTP_ERROR) {
        errno = EIO;
        return -1;
    }

    size_t available =
        abstract->response.length -
            abstract->response.position;
    if (available == 0) {
        return 0; // EOF
    }

    size_t chunk =
        (count < available) ? count : available;
    memcpy(buffer,
        abstract->response.data +
            abstract->response.position,
        chunk);
    abstract->response.position += chunk;
    if (abstract->response.position <
        abstract->response.length) {
        em_http_request_event_set(abstract);
    }
    return chunk;
}

static int em_http_stream_close(php_stream *stream, int type) {
    em_http_abstract_t* abstract =
        (em_http_abstract_t*)stream->abstract;
    
    zend_hash_index_del(
        &__em_http_requests__, abstract->id);

    return SUCCESS;
}

static int em_http_stream_set_option(
    php_stream *stream, int option, int value, void *param) {
    em_http_abstract_t* abstract =
        (em_http_abstract_t*)stream->abstract;

    switch (option) {
        case PHP_STREAM_OPTION_BLOCKING:
            abstract->options.blocking = value;
            return PHP_STREAM_OPTION_RETURN_OK;

        case PHP_STREAM_OPTION_READ_TIMEOUT:
            /* this can only be set in stream
                context options before the request is sent*/
            return PHP_STREAM_OPTION_RETURN_ERR;

        case PHP_STREAM_OPTION_CHECK_LIVENESS:
            if (!(abstract->state & EM_HTTP_COMPLETE)) {
                return PHP_STREAM_OPTION_RETURN_OK;
            }

            if (abstract->state & EM_HTTP_ERROR) {
                return PHP_STREAM_OPTION_RETURN_ERR;
            }

            return (abstract->response.position <
                    abstract->response.length) ?
                PHP_STREAM_OPTION_RETURN_OK :
                    PHP_STREAM_OPTION_RETURN_ERR; /* actually eof */
    }

    return PHP_STREAM_OPTION_RETURN_NOTIMPL;
}

static int em_http_stream_cast(php_stream *stream, int as, void **ret) {
    em_http_abstract_t* abstract = (em_http_abstract_t*)stream->abstract;

    switch (as) { 
        case PHP_STREAM_AS_FD_FOR_SELECT:
            if (ret) {
                memcpy((int*)ret,
                    &abstract->event.pipe[0],
                        sizeof(int));
            }
            return SUCCESS;
 
        default:
            return FAILURE;
    }
}

static php_stream_ops em_http_ops = {
    NULL,                       // write
    em_http_stream_read,        // read  
    em_http_stream_close,       // close
    NULL,                       // flush
    "em-http-nonblock",
    NULL,                       // seek
    em_http_stream_cast,        // cast
    NULL,                       // stat
    em_http_stream_set_option   // set_option
};

void em_http_abstract_free(em_http_abstract_t* abstract) {
    if (--abstract->refcount)
        return;

    if (abstract->response.data) {
        free(abstract->response.data);
    }

    close(abstract->event.pipe[0]);
    close(abstract->event.pipe[1]);

    pefree(abstract, 1);
}

void em_http_abstract_destroy(zval* zv) {
    em_http_abstract_t* abstract =
        (em_http_abstract_t*) Z_PTR_P(zv);

    em_http_abstract_free(abstract);
}

static php_stream *em_http_wrapper_open(php_stream_wrapper *wrapper, 
                                  const char *path, const char *mode,
                                  int options, zend_string **opened_path,
                                  php_stream_context *context STREAMS_DC) {
    em_http_request_t request = em_http_request_create(path, context);

    em_http_abstract_t* abstract =
        pecalloc(1, sizeof(em_http_abstract_t), 1);
    abstract->refcount  = 1; 
    abstract->state     = EM_HTTP_WAITING;
    abstract->id        = zend_hash_num_elements(&__em_http_requests__) + 1;
    abstract->options = (em_http_options_t) {
        .blocking = true
    };

    zend_hash_index_add_ptr(
        &__em_http_requests__,
        abstract->id, abstract);
    abstract->refcount++;

    pipe(abstract->event.pipe);

    if (!em_http_request_start(
            abstract->id,
            request.timeout,
            request.method, request.url,
            (const char**)request.headers.keys,
            (const char**)request.headers.values, 
            request.headers.length,
            request.body, request.length)) {
        zend_hash_index_del(
            &__em_http_requests__, abstract->id);
        em_http_abstract_free(abstract);
        em_http_request_destroy(&request);
        return NULL;
    }

    if (opened_path) {
        *opened_path =
            zend_string_init(path, strlen(path), 0);
    }

    em_http_request_destroy(&request);
    return php_stream_alloc(
        &em_http_ops, abstract, 0, mode);
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

void em_http_startup(void) {
    em_http_request_startup();
}

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

void em_http_shutdown(void) {
    em_http_request_shutdown();
}