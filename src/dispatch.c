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

#include "dispatch.h"
#include "vfs.h"
#include "buffer.h"
#include "api.h"

#include <ext/standard/base64.h>

static zend_always_inline const char* em_dispatch_mime(sapi_request_info* info, const char* fallback) {
    char* extension = strrchr(info->path_translated, '.');

    if (!extension) {
        return fallback;
    }

    if (strcmp(extension, ".css") == SUCCESS) {
        return "text/css; charset=UTF-8";
    } else if (strcmp(extension, ".html") == SUCCESS ||
               strcmp(extension, ".htm")  == SUCCESS) {
        return "text/html; charset=UTF-8";
    }

    return fallback;
}

typedef struct _em_dispatch_t {
    struct {
        const char* type;
        size_t length;
    } mime;
    em_dispatch_handler_t handler;
} em_dispatch_t;

void em_dispatch_header(const char* format, ...) {
    va_list args;
    va_start(args, format);
    sapi_header_line header;

    header.line_len = vspprintf(
        (char **) &(header.line), 0,
        format, args);
    sapi_header_op(
        SAPI_HEADER_REPLACE, (void*) &header);
    efree((void*)header.line);

    va_end(args);
}

static em_dispatch_handler_t em_dispatch_select(const char* mime, sapi_request_info* info);

em_dispatch_handler_t em_dispatch_setup(sapi_request_info* info, const char* method, const char* uri, const char* mime, const char* request, size_t length) {
    SG(server_context) = (void*) &__em_request_buffer;

    memset(info, 0, sizeof(*info));

    info->request_method = method;

    if (!uri || uri[0] != '/') {
        asprintf(
            &info->request_uri,
            "/%s", uri ? uri : "");
    } else {
        info->request_uri = strdup(uri);
    }

    info->content_type   = mime;
    info->content_length = length;

    if (length) {
        em_buffer_request(request, length);
    }

    info->query_string = strstr(info->request_uri, "?");

    /* first we translate request uri into query string and path */
    if (info->query_string) {
        size_t path_translated_length =
            info->query_string - info->request_uri;
        info->path_translated =
            malloc(path_translated_length + 1);
        if (!info->path_translated) {
            /* oom, something else will crash the process, gracefully */
            return NULL;
        }
        memcpy(
            info->path_translated,
            info->request_uri,
            path_translated_length);
        info->path_translated[path_translated_length] = '\0';
        info->query_string++;
    } else {
        info->path_translated = strdup(info->request_uri);
    }

    const char* address = em_vfs_get_address(info->path_translated);

    if (!address) {
        /* then we need to use that path to search for reasonable indexes */
        char search[MAXPATHLEN];
        const char* indexes[] = {
            "index.php",
            "index.html",
            "index.htm",
            NULL
        };
        const char** index = indexes;
        do {
            snprintf(search,
                MAXPATHLEN,
                    "%s%s%s",
                    info->path_translated,
                    info->path_translated[
                        strlen(info->path_translated) - 1
                    ] != '/' ?
                        "/" : "",
                    (*index));
            if (em_vfs_get_address(search)) {
                info->path_translated = strdup(search);
                break;
            }
            index++;
        } while ((*index));
    }

    return em_dispatch_select(mime, info);
}

void em_dispatch_response(int code, const char* status, const char* mime) {
    SG(sapi_headers).http_response_code = code;
    if (SG(sapi_headers).http_status_line) {
        efree(SG(sapi_headers).http_status_line);
    }
    SG(sapi_headers).http_status_line =
        status ? estrdup(status) : NULL;
    if (SG(sapi_headers).mimetype) {
        efree(SG(sapi_headers).mimetype);
    }
    SG(sapi_headers).mimetype =
        mime ? estrdup(mime) : NULL;
}

void em_dispatch_cleanup(sapi_request_info* info) {
    em_buffer_t* buffer =
        (em_buffer_t*)
            SG(server_context);

    free(info->request_uri);
    free(info->path_translated);

    memset(info, 0, sizeof(*info));

    em_buffer_clear(buffer, true);
}

void em_dispatch_error(sapi_request_info* info) {
    em_dispatch_header("Status: 400 Not Found");
    em_dispatch_header("Content-Type: text/plain");
}

void em_dispatch_exception(sapi_request_info* info) {
    em_dispatch_header("Status: 500 Internal Server Error");
    em_dispatch_header("Content-Type: text/plain");
    /* send more content maybe ... */
}

void em_dispatch_api(sapi_request_info* info) {
    em_dispatch_header("Status: 200 OK");
    em_dispatch_header("Content-Type: text/plain");
}

void em_dispatch_file(sapi_request_info* info) {
    /* service path */
    const char* address =
        em_vfs_get_address(info->path_translated);

    if (!address) {
        em_dispatch_error(info);
        return;
    }

    ssize_t length =
        em_vfs_get_length(info->path_translated);

    if (length < 0) {
        em_dispatch_exception(info);
        return;
    }

    em_dispatch_header("Status: 200 OK");
    em_dispatch_header("Content-Type: %s",
        em_dispatch_mime(info,
            "application/octet-stream"));
    em_dispatch_header("Content-Length: %zu", length);
    sapi_send_headers();
    em_buffer_response(address, length);
}

void em_dispatch_script(sapi_request_info* info) {
    em_buffer_t store = EM_BUFFER_EMPTY, load = EM_BUFFER_EMPTY;

    /*
        compile into a new buffer so we don't pollute output
        buffer early, ie, before we know what headers to send
    */
    memcpy(&store,
        &__em_response_buffer, sizeof(em_buffer_t));
    memset(&__em_response_buffer, 0, sizeof(em_buffer_t));

    zend_op_array* ops =
        em_compile_script(
            info->path_translated);

    /*
        put the old buffer back ...
    */
    memcpy(&load,
        &__em_response_buffer, sizeof(em_buffer_t));
    memcpy(&__em_response_buffer, &store, sizeof(em_buffer_t));

    if (!ops) {
        /*
            a bad thing has befallen the request!
        */
        em_dispatch_exception(info);
        sapi_send_headers();
        em_buffer_response(
            load.value, load.length);
        em_buffer_clear(&load, true);
        return;
    }

    em_dispatch_header("Status: 200 OK");
    em_execute(ops);
}

void em_dispatch_base64(sapi_request_info* info) {
    /* service path */
    const unsigned char* address =
        em_vfs_get_address(info->path_translated);

    if (!address) {
        em_dispatch_error(info);
        return;
    }

    ssize_t length =
        em_vfs_get_length(info->path_translated);

    if (length < 0) {
        em_dispatch_exception(info);
        return;
    }

    zend_string* encoded =
        php_base64_encode(address, length);

    em_dispatch_header("Status: 200 OK");
    em_dispatch_header("Content-Type: data/base64");
    em_dispatch_header("Content-Length: %zu", ZSTR_LEN(encoded));
    sapi_send_headers();

    em_buffer_response(
        ZSTR_VAL(encoded), ZSTR_LEN(encoded));
    zend_string_release(encoded);
}

em_dispatch_t __em_dispatch_table__[] = {
    { ZEND_STRL("application/x-em-api"),       em_dispatch_api },
    { ZEND_STRL("application/x-em-file"),      em_dispatch_file },
    { ZEND_STRL("application/x-em-script"),    em_dispatch_script }, 
    { ZEND_STRL("application/x-em-error"),     em_dispatch_error },
    { ZEND_STRL("application/x-em-exception"), em_dispatch_exception },

    { ZEND_STRL("data/base64"),                em_dispatch_base64 },
    { { NULL, 0 }, NULL },
};

static em_dispatch_handler_t em_dispatch_select(const char* mime, sapi_request_info* info) {
    if (mime != NULL) {
        em_dispatch_t* dispatch = __em_dispatch_table__;
        size_t selector = strlen(mime);
        do {
            if (strncmp(mime,
                    dispatch->mime.type,
                    dispatch->mime.length) == 0) {
                return dispatch->handler;
            }
            dispatch++;
        } while (dispatch->handler);
    }

    const char* address = em_vfs_get_address(info->path_translated);

    /* file doesn't exist, handle as error */
    if (!address) {
        return em_dispatch_error;
    }

    /* file extension based dispatch */
    const char* extension = strrchr(info->path_translated, '.');

    /* if no extension, can't be php */
    if (!extension) {
        return em_dispatch_file;
    }

    /* dispatch to script handler */
    if (strcmp(extension, ".php") == SUCCESS) {
        return em_dispatch_script;
    }

    /* fallthrough to file */
    return em_dispatch_file;
}