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

#include <php_variables.h>

#include <ext/standard/base64.h>
#include <ext/standard/php_var.h>
#include <ext/json/php_json.h>

HashTable __em_environ__;

void em_dispatch_file(sapi_request_info* info);
void em_dispatch_script(sapi_request_info* info);
void em_dispatch_code(sapi_request_info* info);
void em_dispatch_api(sapi_request_info* info);
void em_dispatch_exception(sapi_request_info* info);
void em_dispatch_error(sapi_request_info* info);

typedef struct _em_dispatch_header_scan_t {
    struct {
        char data[4096];
        size_t len;
    } key;
    struct {
        char  data[4096];
        size_t len;
    } value;
} em_dispatch_header_scan_t;

#define EM_DISPATCH_HEADER_SCAN_EMPTY      \
    (em_dispatch_header_scan_t) {          \
        .key =   { .data = 0, .len = 0 },  \
        .value = { .data = 0, .len = 0  }  \
}

static void em_dispatch_header_free(em_dispatch_header_t* header) {
    efree(header->key.data);
    efree(header->value.data);
}

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

static zend_always_inline void em_dispatch_nocache(void) {
    em_dispatch_header(
        "Cache-Control: no-store, no-cache, must-revalidate, proxy-revalidate");
    em_dispatch_header("Pragma", "no-cache");
    em_dispatch_header("Expires: 0");
    em_dispatch_header("Connection: close");
}

static zend_always_inline int em_dispatch_readline(em_buffer_t *buffer, const char* format, ...) {
    char *line = strtok_r(
        buffer->token ?
            buffer->token :
            buffer->value,
        "\r\n",
        &buffer->token);

    if (!line) {
        return 0;
    }

    int result;
    va_list args;
    va_start(args, format);
    result = vsscanf(
        line, format, args);
    va_end(args);
    return result;
}

static zend_always_inline void em_dispatch_context_destroy(em_dispatch_context_t* context) {
    em_buffer_clear(&context->buffers.request.head, true);
    em_buffer_clear(&context->buffers.request.body, true);

    em_buffer_clear(&context->buffers.response.head, true);
    em_buffer_clear(&context->buffers.response.body, true);

    zend_llist_destroy(&context->headers.response);
    zend_llist_destroy(&context->headers.request);
    zend_hash_destroy(&context->environ);

    efree(context->info->request_uri);
    if (context->info->content_type) {
        efree(context->info->content_type);
    }
 
    pefree(context, 1);
}

void em_dispatch_env_import(zval* vars) {
    em_dispatch_context_t* context = SG(server_context);

    if (!context) {
        return;
    }

    zend_string* key;
    zval*        value;
    ZEND_HASH_FOREACH_STR_KEY_VAL(&context->environ, key, value) {
        php_register_variable(
            ZSTR_VAL(key),
            Z_STRVAL_P(value),
            vars);
    } ZEND_HASH_FOREACH_END();
}

bool em_dispatch_env(HashTable* table, const char* env, size_t elen, bool persistence) {
    if(!elen) {
        return true;
    }

    zval decoded;
    ZVAL_UNDEF(&decoded);
    if (php_json_decode(
            &decoded, env, elen, true, 512) != SUCCESS) {
        return false;
    }

    if (Z_TYPE(decoded) != IS_ARRAY) {
        return false;
    }

    zend_string* key;
    zval*        value;

    ZEND_HASH_FOREACH_STR_KEY_VAL(
        Z_ARRVAL(decoded), key, value) {
        zval store;

        if (Z_TYPE_P(value) != IS_STRING) {
            convert_to_string(value);
        }

        zend_string* keyed =
            zend_string_init(
                ZSTR_VAL(key),
                ZSTR_LEN(key), persistence);
        zend_string* transient = Z_STR_P(value);
        zend_string* owned =
            zend_string_init(
                ZSTR_VAL(transient),
                ZSTR_LEN(transient), persistence);

        ZVAL_STR(&store, owned);
        zend_hash_update(
            table,
            keyed,
            &store);
        zend_string_release(keyed);
    } ZEND_HASH_FOREACH_END();

    zval_ptr_dtor(&decoded);
    return true;
}

static em_dispatch_context_t*
    em_dispatch_context_create_empty(
        sapi_request_info* info) {
    em_dispatch_context_t* context =
        pecalloc(1, sizeof(em_dispatch_context_t), 1);

    zend_llist_init(
        &context->headers.request,
        sizeof(em_dispatch_header_t),
        (llist_dtor_func_t)
            em_dispatch_header_free, 1);
    zend_llist_init(
        &context->headers.response,
        sizeof(em_dispatch_header_t),
        (llist_dtor_func_t)
            em_dispatch_header_free, 1);

    context->info = info;

    zend_hash_init(
        &context->environ, 8, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_copy(
        &context->environ,
        &__em_environ__,
        zval_copy_ctor);

    return context;
}

static em_dispatch_context_t*
    em_dispatch_context_create(
        sapi_request_info* info,
        const char* env,  size_t elen,
        const char* head, size_t hlen,
        const char* body, size_t blen) {
    em_dispatch_context_t* context =
        pecalloc(1, sizeof(em_dispatch_context_t), 1);

    em_buffer_write(
        &context->buffers.request.head, head, hlen);
    em_buffer_write(
        &context->buffers.request.body, body, blen);

    zend_llist_init(
        &context->headers.request,
        sizeof(em_dispatch_header_t),
        (llist_dtor_func_t)
            em_dispatch_header_free, 1);
    zend_llist_init(
        &context->headers.response,
        sizeof(em_dispatch_header_t),
        (llist_dtor_func_t)
            em_dispatch_header_free, 1);

    context->info = info;

    em_dispatch_header_scan_t scan = EM_DISPATCH_HEADER_SCAN_EMPTY;
    if (em_dispatch_readline(
            &context->buffers.request.head,
            "%[^ ] %[^\r\n]\r\n", /* METHOD URI\r\n */
            &scan.key.data,
            &scan.value.data) != 2) {
        em_dispatch_context_destroy(context);
        return NULL;
    }

    em_dispatch_header_t *initial =
        pecalloc(1, sizeof(em_dispatch_header_t), 1);

    initial->key.data   = estrdup(scan.key.data);
    initial->value.data = estrdup(scan.value.data);

    initial->key.len    = strlen(initial->key.data);
    initial->value.len  = strlen(initial->value.data);

    zend_llist_add_element(&context->headers.request, initial);

    memset(&scan, 0, sizeof(scan));

    while (em_dispatch_readline(&context->buffers.request.head,
            "%[^:]: %[^\r\n]\r\n", /* Header: value\r\n */
            &scan.key.data, &scan.value.data) == 2) {
        em_dispatch_header_t *header =
            pecalloc(1, sizeof(em_dispatch_header_t), 1);
        
        header->key.data   = estrdup(scan.key.data);
        header->value.data = estrdup(scan.value.data);

        header->key.len    = strlen(header->key.data);
        header->value.len  = strlen(header->value.data);

        zend_llist_add_element(&context->headers.request, header);

        memset(&scan, 0, sizeof(scan));
    }

    zend_hash_init(
        &context->environ, 8, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_copy(
        &context->environ,
        &__em_environ__,
        zval_copy_ctor);
    em_dispatch_env(
        &context->environ, env, elen, false);
    return context;
}

em_dispatch_handler_t em_dispatch_setup_script(
    sapi_request_info* info,
    const char* script
) {
    em_dispatch_context_t* context =
        em_dispatch_context_create_empty(info);

    if (!context) {
        return em_dispatch_exception;
    }

    SG(server_context) = context;

    info->path_translated = estrdup(script);

    return em_dispatch_script;
}

em_dispatch_handler_t em_dispatch_setup_code(
    sapi_request_info* info,
    const char* code, size_t length) {
    em_dispatch_context_t* context =
        em_dispatch_context_create_empty(info);

    if (!context) {
        return em_dispatch_exception;
    }

    SG(server_context) = context;

    em_buffer_write(
        &context->buffers.request.body,
        code, length);

    return em_dispatch_code;  
}

em_dispatch_handler_t em_dispatch_setup(
    sapi_request_info* info,
    const char* env,  size_t elen,
    const char* head, size_t hlen,
    const char* body, size_t blen) {
    em_dispatch_context_t* context =
        em_dispatch_context_create(
            info, 
            env, elen,
            head, hlen,
            body, blen);
 
    if (!context) {
        return em_dispatch_exception;
    }

    SG(server_context) =
        (void*) context;
    zend_llist_position position;
    em_dispatch_header_t* header;

    memset(info, 0, sizeof(*info));
    
    header = zend_llist_get_first_ex(
        &context->headers.request, &position);

    if (!header) {
        return em_dispatch_exception;
    }

    info->request_method = estrdup(header->key.data);
    
    if (header->value.data[0] != '/') {
        spprintf(
            &info->request_uri,
            0,
            "/%s",
            header->value.data ?
                header->value.data : "");
    } else {
        info->request_uri = estrdup(header->value.data);
    }

    info->query_string = strstr(info->request_uri, "?");

    /* first we translate request uri into query string and path */
    if (info->query_string) {
        size_t path_translated_length =
            info->query_string - info->request_uri;
        info->path_translated =
            emalloc(path_translated_length + 1);
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
        info->path_translated = estrdup(info->request_uri);
    }

    while ((header = zend_llist_get_next_ex(&context->headers.request, &position))) {
        if (header->key.len == sizeof("content-type")-1 &&
            strncasecmp(header->key.data, "content-type", header->key.len) == 0) {
            info->content_type = estrdup(header->value.data);
        } else if (header->key.len == sizeof("content-length")-1 &&
                   strncasecmp(header->key.data, "content-length", header->key.len) == 0) {
            info->content_length = atol(header->value.data);
        }
    }

    /** check environment for things that effect paths */

    zval* vroot = zend_hash_str_find(
        &context->environ, ZEND_STRL("VIRTUAL_ROOT"));
    zval* droot = zend_hash_str_find(
        &context->environ, ZEND_STRL("DOCUMENT_ROOT"));

    /* strip virtual root from translated path */
    if (vroot && Z_TYPE_P(vroot) == IS_STRING && info->path_translated) {
        size_t vroot_len = Z_STRLEN_P(vroot);
        if (strncmp(info->path_translated, Z_STRVAL_P(vroot), vroot_len) == 0) {
            size_t new_len = strlen(info->path_translated) - vroot_len;
            char* new_path = emalloc(new_len + 1);
            memcpy(new_path, info->path_translated + vroot_len, new_len);
            new_path[new_len] = '\0';
            efree(info->path_translated);
            info->path_translated = new_path;
        }
    }

    /* prepend document root to translated path */
    if (droot && Z_TYPE_P(droot) == IS_STRING && info->path_translated) {
        size_t droot_len = Z_STRLEN_P(droot);
        size_t path_len = strlen(info->path_translated);
        int need_slash = (droot_len && Z_STRVAL_P(droot)[droot_len-1] != '/' && path_len && info->path_translated[0] != '/');
        size_t new_len = droot_len + need_slash + path_len;
        char* new_path = emalloc(new_len + 1);
        memcpy(new_path, Z_STRVAL_P(droot), droot_len);
        if (need_slash) {
            new_path[droot_len] = '/';
            memcpy(new_path + droot_len + 1, info->path_translated, path_len);
        } else {
            memcpy(new_path + droot_len, info->path_translated, path_len);
        }
        new_path[new_len] = '\0';
        efree(info->path_translated);
        info->path_translated = new_path;
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
                efree(
                    info->path_translated);
                info->path_translated = estrdup(search);
                break;
            }
            index++;
        } while ((*index));
    }

    return em_dispatch_select(info->content_type, info);
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

void em_dispatch_cleanup(void) {
    em_dispatch_context_destroy(SG(server_context));
}

void em_dispatch_error(sapi_request_info* info) {
    em_dispatch_header("Status: 400 Not Found");
    em_dispatch_header("Content-Type: text/plain");
    em_dispatch_nocache();
}

void em_dispatch_exception(sapi_request_info* info) {
    em_dispatch_header("Status: 500 Internal Server Error");
    em_dispatch_header("Content-Type: text/plain");
    em_dispatch_nocache();
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
    em_dispatch_nocache();

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

    em_dispatch_header(
        "Status: 200 OK");
    em_dispatch_nocache();
    em_execute(ops);
}

void em_dispatch_code(sapi_request_info* info) {
    em_dispatch_context_t* context = SG(server_context);
    em_buffer_t store = EM_BUFFER_EMPTY, load = EM_BUFFER_EMPTY;

    /*
        compile into a new buffer so we don't pollute output
        buffer early, ie, before we know what headers to send
    */
    memcpy(&store,
        &__em_response_buffer, sizeof(em_buffer_t));
    memset(&__em_response_buffer, 0, sizeof(em_buffer_t));

    zend_op_array* ops =
        em_compile_string(
            context->buffers.request.body.value,
            context->buffers.request.body.length);

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

    em_dispatch_header(
        "Status: 200 OK");
    em_dispatch_nocache();
    em_execute(ops);
}

em_dispatch_t __em_dispatch_table__[] = {
    { ZEND_STRL("application/x-em-api"),       em_dispatch_api },
    { ZEND_STRL("application/x-em-file"),      em_dispatch_file },
    { ZEND_STRL("application/x-em-script"),    em_dispatch_script }, 
    { ZEND_STRL("application/x-em-error"),     em_dispatch_error },
    { ZEND_STRL("application/x-em-exception"), em_dispatch_exception },

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

void em_dispatch_startup(void) {
    zend_hash_init(
        &__em_environ__, 8, NULL, ZVAL_PTR_DTOR, 1);
}

void em_dispatch_shutdown(void) {
    zend_hash_destroy(&__em_environ__);
}