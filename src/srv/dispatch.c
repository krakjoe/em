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
#include <api/api.h>

#include <srv/dispatch.h>
#include <srv/mutators.h>

#include <php_variables.h>

#include <ext/standard/php_var.h>
#include <ext/json/php_json.h>

void em_dispatch_file(em_dispatch_context_t* context);
void em_dispatch_script(em_dispatch_context_t* context);
void em_dispatch_code(em_dispatch_context_t* context);
void em_dispatch_api(em_dispatch_context_t* context);
void em_dispatch_exception(em_dispatch_context_t* context);
void em_dispatch_error(em_dispatch_context_t* context);

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

    /* TODO(krakjoe) more mime */
    if (       strcmp(extension, ".html") == SUCCESS  ||
               strcmp(extension, ".htm")  == SUCCESS  ||
               strcmp(extension, ".md")   == SUCCESS  ||
               strcmp(extension, ".txt")  == SUCCESS) {
        return "text/html; charset=UTF-8";
    } else if (strcmp(extension, ".js")   == SUCCESS  ||
               strcmp(extension, ".mjs")  == SUCCESS  ||
               strcmp(extension, ".wasm") == SUCCESS) {
        return "application/javascript; charset=UTF-8";
    } else if (strcmp(extension, ".css") == SUCCESS) {
        return "text/css; charset=UTF-8";
    } else if (strcmp(extension, ".yml") == SUCCESS ||
               strcmp(extension, ".yaml") == SUCCESS) {
        return "text/yaml; charset=UTF-8";
    } else if (strcmp(extension, ".svg") == SUCCESS) {
        return "image/svg+xml; charset=UTF-8";
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

void em_dispatch_header(em_dispatch_context_t* context, const char* format, ...) {
    va_list args;
    va_start(args, format);
    sapi_header_line header;

    header.line_len = vspprintf(
            (char **) &(header.line), 0,
            format, args);

    if (!SG(headers_sent)) {
        sapi_header_op(
            SAPI_HEADER_REPLACE, (void*) &header);
    } else if (context->buffers.response.head.length) {
        /**
         * We must allow very late editing of the response
         * header to accomodate mutation.
         * 
         * The sapi may or may not think the headers have been
         * sent at call time, its safe to ignore if they have
         * been sent, since sending doesn't exist we can edit
         * the buffer until its joined on return to javascript.
         */
        const char* name = strchr(header.line, ':');

        if (name) {
            char *search =
                malloc((name - header.line) + 1);
            memcpy(search,
                header.line, name - header.line);
            search[(name - header.line)] = 0;

            char *line = strcasestr(
                context->buffers.response.head.value, search);
            if (line) {
                // Find the end of the header line (\r\n)
                char *end = strstr(line, "\r\n");

                if (end) {
                    size_t blength =
                        line - context->buffers.response.head.value;
                    size_t offset =
                        (end + 2) - context->buffers.response.head.value;
                    size_t alength =
                        context->buffers.response.head.length - offset;
                    size_t length = strlen(header.line);

                    // Allocate new buffer for the header block
                    size_t nlength = blength + length + 2 + alength;
                    char *nbuffer = emalloc(nlength + 1);

                    // Copy up to the start of the line
                    memcpy(nbuffer,
                        context->buffers.response.head.value,
                        blength);
                    // Copy the new header line
                    memcpy(nbuffer + blength,
                        header.line, 
                        length);
                    // Copy CRLF
                    memcpy(nbuffer + blength + length, "\r\n", 2);
                    // Copy the rest of the buffer
                    memcpy(nbuffer + blength + length + 2,
                        context->buffers.response.head.value + offset,
                        alength);
                    nbuffer[nlength] = '\0';

                    em_buffer_clear(&context->buffers.response.head, true);
                    em_buffer_write(&context->buffers.response.head, nbuffer, nlength);
                    efree(nbuffer);
                }
            }
            free(search);
        }
    }

    efree((void*)header.line);

    va_end(args);
}

static em_dispatch_handler_t em_dispatch_select(const char* mime, sapi_request_info* info);

static zend_always_inline void em_dispatch_nocache(em_dispatch_context_t* context) {
    em_dispatch_header(context,
        "Cache-Control: no-store, no-cache, must-revalidate, proxy-revalidate");
    em_dispatch_header(context, "Pragma", "no-cache");
    em_dispatch_header(context, "Expires: 0");
    em_dispatch_header(context, "Connection: close");
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

void em_dispatch_free(em_dispatch_context_t* context) {
    em_buffer_clear(&context->buffers.request.head, true);
    em_buffer_clear(&context->buffers.request.body, true);

    em_buffer_clear(&context->buffers.response.head, true);
    em_buffer_clear(&context->buffers.response.body, true);
    em_buffer_clear(&context->buffers.response.join, true);

    zend_llist_destroy(&context->headers.response);
    zend_llist_destroy(&context->headers.request);
    zend_hash_destroy(&context->environ);

    em_url_free(&context->url);

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
        sapi_request_info* info,
        em_run_reaper_t reaper) {
    em_dispatch_context_t* context =
        pecalloc(1, sizeof(em_dispatch_context_t), 1);
    memset(context, 0, sizeof(em_dispatch_context_t));

    context->previous = (em_dispatch_context_previous_t) {
        .context = SG(server_context),
        .info    = SG(request_info)
    };

    context->reaper = reaper;
    context->info   = info;

    em_buffer_clear(
        &context->buffers.request.head, false);
    em_buffer_clear(
        &context->buffers.request.body, false);

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
        const char* body, size_t blen,
        em_run_reaper_t reaper) {
    em_dispatch_context_t* context =
        pecalloc(1, sizeof(em_dispatch_context_t), 1);
    memset(context, 0, sizeof(em_dispatch_context_t));

    context->previous = (em_dispatch_context_previous_t) {
        .context = SG(server_context),
        .info    = SG(request_info)
    };

    context->reaper = reaper;
    context->info   = info;

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

    em_dispatch_header_scan_t scan = EM_DISPATCH_HEADER_SCAN_EMPTY;
    if (em_dispatch_readline(
            &context->buffers.request.head,
            "%[^ ] %[^\r\n]\r\n", /* METHOD URI\r\n */
            &scan.key.data,
            &scan.value.data) != 2) {
        context->handler =
            em_dispatch_exception;
        return context;
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

    if (!em_url_parse(context, initial->value.data, &context->url)) {
        fprintf(stderr, "[dispatch] could not parse url\n");
        context->handler =
            em_dispatch_exception;
        return context;
    }

    return context;
}

em_dispatch_context_t* em_dispatch_enter_script(
    sapi_request_info* info,
    const char* script,
    em_run_reaper_t reaper
) {
    em_dispatch_context_t* context =
        em_dispatch_context_create_empty(
            info, reaper);

    info->path_translated = estrdup(script);

    context->handler =
        em_dispatch_script;

    return (SG(server_context) = context);
}

em_dispatch_context_t* em_dispatch_enter_code(
    sapi_request_info* info,
    const char* code, size_t length,
    em_run_reaper_t reaper) {
    em_dispatch_context_t* context =
        em_dispatch_context_create_empty(
            info, reaper);

    em_buffer_write(
        &context->buffers.request.body,
        code, length);

    context->handler =
        em_dispatch_code;

    return (SG(server_context) = context);
}

em_dispatch_context_t* em_dispatch_enter(
    sapi_request_info* info,
    const char* env,  size_t elen,
    const char* head, size_t hlen,
    const char* body, size_t blen,
    em_run_reaper_t reaper) {
    em_dispatch_context_t* context =
        em_dispatch_context_create(
            info, 
            env, elen,
            head, hlen,
            body, blen,
            reaper);
    zend_llist_position position;
    em_dispatch_header_t* header;

    memset(info, 0, sizeof(*info));

    header = zend_llist_get_first_ex(
        &context->headers.request, &position);

    if (!header) {
        context->handler =
            em_dispatch_exception;
        return context;
    }

    info->request_method =
        estrdup(header->key.data);
    info->request_uri     = estrdup(context->url.uri);
    info->query_string    = context->url.query ?
                                estrdup(context->url.query) :
                                    NULL;
    info->path_translated = estrdup(context->url.path.vfs);

    while ((header = zend_llist_get_next_ex(&context->headers.request, &position))) {
        if (header->key.len == sizeof("content-type")-1 &&
            strncasecmp(header->key.data, "content-type", header->key.len) == 0) {
            info->content_type = estrdup(header->value.data);
        } else if (header->key.len == sizeof("content-length")-1 &&
                   strncasecmp(header->key.data, "content-length", header->key.len) == 0) {
            info->content_length = atol(header->value.data);
        }
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

    context->handler =
        em_dispatch_select(info->content_type, info);

    return (SG(server_context) = context);
}

size_t em_dispatch_response(em_dispatch_context_t* context, em_dispatch_selector_t selected, const char* buffer, size_t length) {
    switch (selected) {
        case EM_DISPATCH_HEAD:
            return em_buffer_write(&context->buffers.response.head, buffer, length);
        case EM_DISPATCH_BODY:
            return em_buffer_write(&context->buffers.response.body, buffer, length);
    }
}

em_dispatch_context_t* em_dispatch_leave(em_dispatch_context_t* context) {
    /** First we join this contexts buffer **/
    em_buffer_join(&context->buffers.response.join,
        &context->buffers.response.head,
        &context->buffers.response.body);

    /** Then we must restore the previous context and info */
    SG(server_context) =
        context->previous.context;
    memcpy(
        &SG(request_info),
        &context->previous.info,
        sizeof(sapi_request_info));
    return context;
}

void em_dispatch_error(em_dispatch_context_t* context) {
    fprintf(stderr,
        "[dispatch] error for %p\n", context);
    em_dispatch_header(context, "Status: 400 Not Found");
    em_dispatch_header(context, "Content-Type: text/plain");
    em_dispatch_nocache(context);
}

void em_dispatch_exception(em_dispatch_context_t* context) {
    fprintf(stderr,
        "[dispatch] exception for %p\n", context);
    em_dispatch_header(context, "Status: 500 Internal Server Error");
    em_dispatch_header(context, "Content-Type: text/plain");
    em_dispatch_nocache(context);
}

void em_dispatch_api(em_dispatch_context_t* context) {
    em_dispatch_header(context, "Status: 200 OK");
    em_dispatch_header(context, "Content-Type: text/plain");
}

void em_dispatch_file(em_dispatch_context_t* context) {

    const char* address =
        em_vfs_get_address(context->info->path_translated);

    if (!address) {
        em_dispatch_error(context);
        return;
    }

    ssize_t length =
        em_vfs_get_length(context->info->path_translated);

    if (length < 0) {
        em_dispatch_exception(context);
        return;
    }

    em_dispatch_header(context, "Status: 200 OK");
    em_dispatch_header(context, "Content-Type: %s",
        em_dispatch_mime(context->info,
            "application/octet-stream"));
    em_dispatch_header(context, "Content-Length: %zu", length);
    em_dispatch_nocache(context);
    em_dispatch_response(context,
        EM_DISPATCH_BODY, address, length);
    em_mutators_mutate(context);
}

void em_dispatch_script(em_dispatch_context_t* context) {
    em_vfs_path_t* vpath =
        em_vfs_mkpath(
            context->info->path_translated, false);

    chdir(vpath->directory);
    em_vfs_path_release(vpath);

    zend_op_array* ops =
        em_compile_script(
            context->info->path_translated);

    if (!ops) {
        em_dispatch_exception(context);
        return;
    }

    em_dispatch_nocache(context);

    em_execute(ops);

    em_mutators_mutate(context);
}

void em_dispatch_code(em_dispatch_context_t* context) {
    zend_op_array* ops =
        em_compile_string(
            context->buffers.request.body.value,
            context->buffers.request.body.length);

    if (!ops) {
        em_dispatch_exception(context);
        return;
    }

    em_dispatch_nocache(context);
    em_execute(ops);
    em_mutators_mutate(context);
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

}

void em_dispatch_shutdown(void) {

}