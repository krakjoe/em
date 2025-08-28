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

#include <http/http.h>
#include <http/request.h>

HashTable __em_http_requests__;

void em_http_request_startup(void) {
    zend_hash_init(
        &__em_http_requests__, 8, NULL,
        em_http_abstract_destroy, 1);
}

static void em_http_request_headers_parse_line(em_http_request_t* request, const char* line, size_t* count) {
    // Find the colon separator
    const char* colon = strchr(line, ':');
    if (!colon) {
        return;
    }

    // Split and trim the key
    size_t key_len = colon - line;
    while ((key_len > 0) &&
          (line[key_len-1] == ' ' || line[key_len-1] == '\t'))
        key_len--;
    if (key_len == 0) {
        return;
    }

    // Skip colon and whitespace for value
    const char* value = colon + 1;
    while (*value == ' ' || *value == '\t') value++;
    size_t value_len = strlen(value);
    while ((value_len > 0) &&
           (value[value_len-1] == ' ' || value[value_len-1] == '\t'))
        value_len--;
    if (value_len == 0) {
        return;
    }

    // Store the header
    request->headers.keys[*count] =
        pestrndup(line, key_len, 1);
    request->headers.values[*count] =
        pestrndup(value, value_len, 1);
    (*count)++;
}

static void em_http_request_headers_array(em_http_request_t* request, zval* headers) {
    // First count valid string elements
    size_t count = 0;
    zval* value;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(headers), value) {
        if (value && Z_TYPE_P(value) == IS_STRING) {
            count++;
        }
    } ZEND_HASH_FOREACH_END();

    if (count == 0) {
        request->headers.length = 0;
        request->headers.keys = NULL;
        request->headers.values = NULL;
        return;
    }

    request->headers.keys = pecalloc(sizeof(char*), count, 1);
    request->headers.values = pecalloc(sizeof(char*), count, 1);
    
    size_t header_count = 0;
    zend_string* key;
    ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(headers), key, value) {
        if (!key ||
            !value ||
            Z_TYPE_P(value) != IS_STRING) {
            /* the only supported form for header arrays is key => value */
            continue;
        }

        request->headers.keys[header_count] =
            pestrndup(ZSTR_VAL(key), ZSTR_LEN(key), 1);
        request->headers.values[header_count] =
            pestrndup(Z_STRVAL_P(value), Z_STRLEN_P(value), 1);
        header_count++;
    } ZEND_HASH_FOREACH_END();
    
    request->headers.length = header_count;
}

static void em_http_request_headers_string(em_http_request_t* request, zval* headers) {
    // Count number of \r\n delimiters to allocate space
    const char* str = Z_STRVAL_P(headers);
    size_t len = Z_STRLEN_P(headers);
    size_t count = 1;  // At least one header even without delimiters
    
    for (size_t i = 0; i < len - 1; i++) {
        if (str[i] == '\r' && str[i + 1] == '\n') {
            count++;
        }
    }

    request->headers.keys = pecalloc(sizeof(char*), count, 1);
    request->headers.values = pecalloc(sizeof(char*), count, 1);

    // Now parse each line
    char* parsing = estrndup(str, len);
    char* line = strtok(parsing, "\r\n");
    size_t counted = 0;

    while (line) {
        // Skip empty lines
        if (*line) {
            em_http_request_headers_parse_line(
                request, line, &counted);
        }
        line = strtok(NULL, "\r\n");
    }

    efree(parsing);
    request->headers.length = counted;
}

em_http_request_t em_http_request_create(const char* url, php_stream_context* context) {
    zval *option;
    em_http_request_t request = {
        .method  = NULL,
        .url     = pestrdup(url, 1),
        .headers = {
            .keys   = NULL,
            .values = NULL,
            .length = 0,
        },
        .body    = NULL,
        .length  = 0,
        .timeout = 60 * 1000,
    };

    if (!context || Z_TYPE(context->options) != IS_ARRAY) {
        /**
         * Where no context is available the default must be GET
         */
        request.method =
            pestrdup("GET", 1);
        return request;
    }

    option = php_stream_context_get_option(context, "http", "method");

    if (option && Z_TYPE_P(option) == IS_STRING) {
        request.method = pestrdup(Z_STRVAL_P(option), 1);
    } else {
        request.method = pestrdup("GET", 1);
    }

    option = php_stream_context_get_option(context, "http", "header");

    if (option) {
        if (Z_TYPE_P(option) == IS_ARRAY) {
            em_http_request_headers_array(&request, option);
        } else if (Z_TYPE_P(option) == IS_STRING) {
            em_http_request_headers_string(&request, option);
        }
    }

    option = php_stream_context_get_option(context, "http", "content");

    if (option && Z_TYPE_P(option) == IS_STRING) {
        request.body   = Z_STRVAL_P(option);
        request.length = Z_STRLEN_P(option);
    }

    option = php_stream_context_get_option(context, "http", "timeout");

    if (option &&
        (Z_TYPE_P(option) == IS_LONG || Z_TYPE_P(option) == IS_DOUBLE)) {
        convert_to_double(option);
        request.timeout =
            Z_DVAL_P(option) * 1000;
    }

    return request;
}

EM_JS(int, em_http_request_start, (
    int id,
    uint32_t timeout,
    const char* method,
    const char* url,
    const char** hkeys, 
    const char** hvalues,
    size_t hlength,
    const char* body,
    size_t blength), {

    const xhr = new XMLHttpRequest();
 
    const http = {
        id:     id,
        method: method ? UTF8ToString(method) : 'GET',
        url:    UTF8ToString(url),
        xhr:    xhr
    };

    Module.http.set(id, http);

    if (timeout) {
       // xhr.timeout = timeout;
    }

    xhr.open(http.method, http.url, true);
    xhr.responseType = 'arraybuffer';

    // Set headers
    if (hlength > 0) {
        for (let i = 0; i < hlength; i++) {
            const key = UTF8ToString(Module.HEAP32[(hkeys >> 2) + i]);
            const value = UTF8ToString(Module.HEAP32[(hvalues >> 2) + i]);
            if (key.toLowerCase() !== 'user-agent') {
                xhr.setRequestHeader(key, value);
            }
        }
    }

    xhr.onreadystatechange = function() {
        if (xhr.readyState === XMLHttpRequest.DONE) {
            let response = new Uint8Array();

            if (xhr.status >= 200 && xhr.status < 300 && xhr.response) {
                response = new Uint8Array(xhr.response);
            }

            let buffer =
                Module._malloc(response.byteLength);

            if (response.byteLength) {
                Module.HEAPU8.set(response, buffer);
            }

            Module.ccall('em_http_request_response', null, 
                ['number', 'number', 'number', 'number'], 
                [id, xhr.status, buffer, response.byteLength]);

            Module.http.delete(id);
        }
    };

    xhr.onerror = function() {
        
        Module.ccall('em_http_request_error', null, ['number'], [id]);
    };

    // Prepare and send body
    let sendData = null;
    if (blength > 0) {
        sendData = new Uint8Array(blength);
        for (let i = 0; i < blength; i++) {
            sendData[i] = Module.HEAPU8[body + i];
        }
    }

    xhr.send(sendData);
    return 1; // Started successfully
});

void EMSCRIPTEN_KEEPALIVE em_http_request_response(
    int id, int status, uintptr_t buffer, size_t length) {
    em_http_abstract_t* abstract =
        (em_http_abstract_t*)
            zend_hash_index_find_ptr(
                &__em_http_requests__, id);
    if (status >= 200 && status < 300) {
        abstract->state = EM_HTTP_COMPLETE;
    } else {
        abstract->state =
            EM_HTTP_COMPLETE | EM_HTTP_ERROR;
    }

    abstract->response.position = 0;
    abstract->response.length   = length;
    abstract->response.data     = (char*) buffer;

    em_http_request_event_set(abstract);
}

void EMSCRIPTEN_KEEPALIVE em_http_request_timeout(int id) {
    em_http_abstract_t* abstract =
        (em_http_abstract_t*)
            zend_hash_index_find_ptr(
                &__em_http_requests__, id);
    abstract->state =
        EM_HTTP_COMPLETE | 
        EM_HTTP_ERROR | 
        EM_HTTP_TIMEOUT;
    em_http_request_event_set(abstract);
}

void EMSCRIPTEN_KEEPALIVE em_http_request_error(int id) {
    em_http_abstract_t* abstract =
        (em_http_abstract_t*)
            zend_hash_index_find_ptr(
                &__em_http_requests__, id);
    abstract->state = 
        EM_HTTP_COMPLETE |
        EM_HTTP_ERROR;
    em_http_request_event_set(abstract);
}

void em_http_request_destroy(em_http_request_t* request) {
    if (request->method) {
        pefree(request->method, 1);
    }
    if (request->url) {
        pefree(request->url, 1);
    }
    if (request->headers.length) {
        for (size_t header = 0;
                    header < request->headers.length;
                    header++) {
            pefree(request->headers.keys[header], 1);
            pefree(request->headers.values[header], 1);
        }
        pefree(request->headers.keys, 1);
        pefree(request->headers.values, 1);
    }
}

void em_http_request_event_set(em_http_abstract_t* abstract) {
    abstract->event.drained = false;
    write(
        abstract->event.pipe[1],
        "\0",
        sizeof(char));
}

void em_http_request_event_clear(em_http_abstract_t* abstract) {
    if (!abstract->event.drained) {
        abstract->event.drained = true;
        read(abstract->event.pipe[0],
            &abstract->event.drain, sizeof(char));
    }
}

void em_http_request_shutdown(void) {
    zend_hash_destroy(
        &__em_http_requests__);
}