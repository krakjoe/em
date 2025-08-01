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

static void em_http_request_headers_array(em_http_request_t* request, zval* headers) {
    request->headers.length =
        zend_hash_num_elements(Z_ARRVAL_P(headers));
    request->headers.keys = pecalloc(
        sizeof(char*), request->headers.length, 1);
    request->headers.values = pecalloc(
        sizeof(char*), request->headers.length, 1);

    zend_string* key;
    zval*        value;
    size_t       header = 0;
    ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(headers), key, value) {
        if (!key || /* non-numeric index */
            !value || /* null value */
            Z_TYPE_P(value) != IS_STRING) { /* malformed array */
            request->headers.keys[header] = pestrdup("", 1);
            request->headers.values[header] = pestrdup("", 1);
            header++;
            continue;
        }

        request->headers.keys[header] = pestrndup(
            ZSTR_VAL(key), ZSTR_LEN(key), 1);
        request->headers.values[header] = pestrndup(
            Z_STRVAL_P(value), Z_STRLEN_P(value), 1);
        header++;
    } ZEND_HASH_FOREACH_END();
}

static void em_http_request_headers_string(em_http_request_t* request, zval* headers) {
    // Parse string headers into array format and delegate
    zval array;
    array_init(&array);

    // Work with a copy so we can modify it
    char* parsing = estrdup(Z_STRVAL_P(headers));
    char* line    = strtok(parsing, "\n\r");

    while (line) {
        // Skip empty lines and trim whitespace
        while (*line == ' ' || *line == '\t') line++;
        if (*line == '\0') {
            line = strtok(NULL, "\n\r");
            continue;
        }
        
        // Find the colon separator
        char* colon = strchr(line, ':');
        if (colon) {
            // Split into key and value
            *colon = '\0';  // Null terminate key
            char* key = line;
            char* value = colon + 1;

            // Trim key
            char* key_end = colon - 1;
            while (key_end > key && (*key_end == ' ' || *key_end == '\t')) {
                *key_end = '\0';
                key_end--;
            }

            // Trim value
            while (*value == ' ' || *value == '\t') value++;
            char* value_end = value + strlen(value) - 1;
            while (value_end > value && (*value_end == ' ' || *value_end == '\t')) {
                *value_end = '\0';
                value_end--;
            }

            // Add to array if both key and value are non-empty
            if (*key && *value) {
                add_assoc_string(&array, key, value);
            }
        }

        line = strtok(NULL, "\n\r");
    }

    efree(parsing);
    
    // Delegate to array function if we have headers
    if (zend_hash_num_elements(Z_ARRVAL(array)) > 0) {
        em_http_request_headers_array(request, &array);
    }
    
    zval_ptr_dtor(&array);
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

    option = php_stream_context_get_option(context, "http", "method");

    if (option && Z_TYPE_P(option) == IS_STRING) {
        request.method = pestrdup(Z_STRVAL_P(option), 1);
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
        Z_TYPE_P(option) == IS_LONG ||
        Z_TYPE_P(option) == IS_DOUBLE) {
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
                    Module.HEAP32[
                        (http.headers.keys   >> 2) + h]);
                let value = UTF8ToString(
                    Module.HEAP32[
                        (http.headers.values >> 2) + h]);

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