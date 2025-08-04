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

#include "http.h"
#include "request.h"

static void em_http_request_headers_parse_line(em_http_request_t* request, const char* line, size_t* header_count) {
    // Find the colon separator
    const char* colon = strchr(line, ':');
    if (!colon) return;

    // Split and trim the key
    size_t key_len = colon - line;
    while (key_len > 0 && (line[key_len-1] == ' ' || line[key_len-1] == '\t')) key_len--;
    if (key_len == 0) return;

    // Skip colon and whitespace for value
    const char* value = colon + 1;
    while (*value == ' ' || *value == '\t') value++;
    size_t value_len = strlen(value);
    while (value_len > 0 && (value[value_len-1] == ' ' || value[value_len-1] == '\t')) value_len--;
    if (value_len == 0) return;

    // Store the header
    request->headers.keys[*header_count] = pestrndup(line, key_len, 1);
    request->headers.values[*header_count] = pestrndup(value, value_len, 1);
    (*header_count)++;
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
    size_t header_count = 0;

    while (line) {
        // Skip empty lines
        if (*line) {
            em_http_request_headers_parse_line(request, line, &header_count);
        }
        line = strtok(NULL, "\r\n");
    }

    efree(parsing);
    request->headers.length = header_count;
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

EM_JS(ssize_t, em_http_request, (
    const char*  method,
    const char*  url,
    const char** hkeys,
    const char** hvalues,
    size_t       hlength,
    const char*  body,
    size_t       blength,
    size_t       timeout,
    uintptr_t abstract), {

    let http = {
        method:  method  ?
            UTF8ToString(method)  : 'GET',
        url: UTF8ToString(url),
        headers: {
            keys:   hkeys,
            values: hvalues,
            length: hlength
        },
        body:     null,
        blength:  blength,
        timeout:  timeout,
        xhr:      new XMLHttpRequest(),
    };

    if (http.blength) {
        http.body = "";
        for (let b = 0; b < http.blength; b++) {
            http.body += String.fromCharCode(
                Module.HEAPU8[body + b]);
        }
    }

    if (Module.dispatchEvent) {
        Module.dispatchEvent(new CustomEvent('io.begin', {
            detail: http
        }));
    }

    try {
        http.xhr.open(
            http.method, http.url, false);

        // set headers after open()
        if (http.headers.length) {
            for (let h = 0; h < http.headers.length; h++) {
                let key = UTF8ToString(
                    Module.getValue(http.headers.keys + h * 4, 'i32'));
                let value = UTF8ToString(
                    Module.getValue(http.headers.values + h * 4, 'i32'));

                if (key.toLowerCase() == "user-agent") {
                    console.warn(
                        "illegal to set user-agent, ignoring");
                    continue;
                }

                http.xhr.setRequestHeader(key, value);
            }
        }

        // x-user-defined is mysterious:
        // this tells javascript not to mess with the stream essentially
        http.xhr.overrideMimeType('text/plain; charset=x-user-defined');

        http.xhr.send(http.body);

        if (http.xhr.status >= 200 && http.xhr.status < 300) {
            var response = http.xhr.responseText;
            var length   = response.length;
            var buffer   = Module.
                _em_http_buffer(abstract, length);

            for (var i = 0; i < length; i++) {
                Module.HEAPU8[buffer + i] =
                    response.charCodeAt(i) & 0xFF;
            }

            if (Module.dispatchEvent) {
                Module.dispatchEvent(new CustomEvent('io.end', {
                    detail: {
                        address: buffer,
                        length:  length,
                        ...http
                    }
                }));
            }

            return length;
        } else {
            if (Module.dispatchEvent) {
                Module.dispatchEvent(new CustomEvent('io.error', {
                    detail: http
                }));
            }
        }
    } catch (exception) {
        if (Module.dispatchEvent) {
            Module.dispatchEvent(new CustomEvent('io.exception', {
                detail: {
                    exception: exception,
                    ...http
                }
            }));
        }

        console.error('Fetch failed:', exception);
    }
    return -1;
});