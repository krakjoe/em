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

#include <SAPI.h>

#include <php_main.h>
#include <php_variables.h>
#include <zend_exceptions.h>
#include <ext/json/php_json.h>

#include "buffer.h"
#include "vfs.h"
#include "http.h"
#include "api.h"
#include "dispatch.h"
#include "mutators.h"
#include "stdxx.h"
#include "proc.h"

#ifdef HAVE_EM_SHM
extern void em_shm_startup(void);
extern void em_shm_shutdown(void);
#endif

extern sapi_module_struct em_sapi_module;

#if PHP_VERSION_ID >= 80500
#define EM_INI_OPCACHE \
    "opcache.enable=1\n"
#else
#define EM_INI_OPCACHE \
    "opcache.enable=0\n"
#endif

static const char EM_INI[] =
    "variables_order=EGPCS\n"
    EM_INI_OPCACHE
    "allow_url_fopen=1\n"
    "allow_url_include=1\n"
    "html_errors=0\n"
    "error_reporting=E_ALL &~ E_DEPRECATED\n"
    "display_errors=1\n"
    "register_argc_argv=0\n"
    "implicit_flush=1\n"
    "output_buffering=0\n"
    "max_execution_time=0\n"
    "max_input_time=-1\n\0";

typedef zend_op_array* (*zend_compile_func_t)(
    zend_file_handle* fh,
    int  type);
#if PHP_VERSION_ID < 80100
typedef void (*zend_error_func_t)(
    int type,
    const char *file,
    const uint32_t line,
    zend_string *message);
#else
typedef void (*zend_error_func_t)(
    int type,
    zend_string *file,
    const uint32_t line,
    zend_string *message);
#endif
typedef void (*zend_log_func_t)(
    const char *message,
    int type);
typedef size_t (*zend_write_func_t)(
    const char* buf,
    size_t len);

static zend_compile_func_t zend_compile_func;
static zend_write_func_t   zend_write_func;
static zend_error_func_t   zend_error_func;
static zend_log_func_t     zend_log_func;

static zend_op_array*
    em_compile_file(
        zend_file_handle* fh, int type);

/* {{{ logging */
void em_buffer_log(const char* message, int type) {
    zend_string* msg = zend_strpprintf(
        0, "em internal error: %s\n",
        message
    );

    em_dispatch_response(SG(server_context), EM_DISPATCH_BODY, ZSTR_VAL(msg), ZSTR_LEN(msg));
    zend_string_release(msg);
}

#if PHP_VERSION_ID < 80100
void em_buffer_error(int type, const char* file, const uint32_t lineno, zend_string* message) {
    zend_string* msg = zend_strpprintf(
        0, "em error in %s on line %u: %s\n",
        file,
        lineno,
        ZSTR_VAL(message)
    );
    em_dispatch_response(SG(server_context), EM_DISPATCH_BODY, ZSTR_VAL(msg), ZSTR_LEN(msg));
    zend_string_release(msg);
}
#else
void em_buffer_error(int type, zend_string* file, const uint32_t lineno, zend_string* message) {
    zend_string* msg = zend_strpprintf(
        0, "em error in %s on line %u: %s\n",
        ZSTR_VAL(file),
        lineno,
        ZSTR_VAL(message)
    );
    em_dispatch_response(SG(server_context), EM_DISPATCH_BODY, ZSTR_VAL(msg), ZSTR_LEN(msg));
    zend_string_release(msg);
}
#endif /* }}} */

/* {{{ code lifecycle management */
static zend_always_inline zend_result
    em_activate(bool headers) {

    if (php_request_startup() != SUCCESS) {
        php_module_shutdown();

        return FAILURE;
    }

    SG(headers_sent)            = !headers;
    SG(request_info).no_headers = !headers;

    if (headers) {
        /* alway set a valid status code for response
            in case the system goes down during startup */
        SG(sapi_headers).http_response_code = 200;
        SG(sapi_headers).http_status_line   = estrdup("OK");
    }

    em_mutators_activate();
    em_http_activate();
    em_vfs_activate();

    EG(full_tables_cleanup) = 1;

    return SUCCESS;
}

static long em_string_read(void *handle, char *buf, size_t len) {
    em_buffer_t* string =
    	(em_buffer_t*)handle;
 
    if (len > string->length) {
    	len = string->length;
    }

    if (len == 0) {
        return len;
    }

    memcpy(
        buf, string->value, len);

    return len;
}

static size_t em_string_length(void *handle) {
    em_buffer_t* string =
    	(em_buffer_t*) handle;
    return string->length;
}

static void em_string_close(void *handle) { /* no op */ }

static zend_always_inline
    zend_op_array*
        em_compile_file(zend_file_handle *fh, int type) {
    zend_error_func   = zend_error_cb;
    zend_error_cb     = em_buffer_error;

    zend_op_array* compiled = NULL;
    zend_try {
        compiled = zend_compile_func(fh, type);
    } zend_end_try();

    if (EG(exception)) {
        zend_exception_error(
            EG(exception), E_COMPILE_ERROR);
    }

    zend_error_cb     = zend_error_func;
 
    return compiled;
}

zend_op_array* em_compile_string(const char* code, size_t length) {
    em_buffer_t string =
    	(em_buffer_t) {
    	    .value   = (char*) code,
    	    .length  = length
    };

    zend_file_handle fh;
    zend_stream_init_filename(
    	&fh, "stdin.php");
    fh.type = ZEND_HANDLE_STREAM;
    fh.handle.stream.handle = (void*)&string;
    fh.handle.stream.reader = em_string_read;
    fh.handle.stream.closer = em_string_close;
    fh.handle.stream.fsizer = em_string_length;
    fh.handle.stream.isatty = 0;
    zend_op_array* compiled =
        zend_compile_file(
            &fh, ZEND_INCLUDE);
    zend_destroy_file_handle(&fh);
    return compiled;
}

zend_op_array* em_compile_script(const char* script) {
    zend_file_handle fh;
    zend_stream_init_filename(&fh, script);
    fh.type = ZEND_HANDLE_FILENAME;
    fh.handle.stream.isatty = 0;
    zend_op_array* compiled =
        zend_compile_file(
            &fh, ZEND_INCLUDE);
    zend_destroy_file_handle(&fh);
    return compiled;
}

void em_execute(zend_op_array* ops) {
    zval retval;
    ZVAL_UNDEF(&retval);

    zend_try {
        zend_execute(ops, &retval);
    } zend_end_try();

    if (EG(exception)) {
        zend_exception_error(EG(exception), E_ERROR);
    }

    destroy_op_array(ops);
    efree_size(ops, sizeof(zend_op_array));
}

static zend_always_inline void em_deactivate(void) {
    em_vfs_deactivate();
    em_http_deactivate();
    em_mutators_deactivate();

    php_request_shutdown((void*) NULL);
} /* }}} */

/* {{{ exports */
int EMSCRIPTEN_KEEPALIVE em_startup(void) {
#ifdef HAVE_EM_SHM
    em_shm_startup();
#endif
    /**
     * We don't expect to leak, this is not leak suppression
     * We are stopping uaf at leak checker
     */
    putenv("USE_ZEND_ALLOC=0");

#ifdef ZTS
    php_tsrm_startup();
#ifdef _WIN32
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
#endif

    zend_signal_startup();

    sapi_startup(&em_sapi_module);

    em_sapi_module.ini_entries =
        malloc(sizeof(EM_INI));
    memcpy((void*)em_sapi_module.ini_entries,
        EM_INI, sizeof(EM_INI));

    /* do not attempt to scan search paths for ini */
    em_sapi_module.php_ini_ignore = 1;

  	if (em_sapi_module.startup(&em_sapi_module) == FAILURE) {
  		return FAILURE;
  	}

    SG(options)                |= SAPI_OPTION_NO_CHDIR;

    zend_log_func   = sapi_module.log_message;
    zend_write_func = sapi_module.ub_write;

    sapi_module.ub_write    = em_dispatch_writer;
    sapi_module.log_message = em_buffer_log;

    em_proc_startup();
    em_stdxx_startup();
    em_dispatch_startup();
    em_http_startup();
    em_vfs_startup();

    php_import_environment_variables = em_dispatch_env_import;

    zend_compile_func = zend_compile_file;
    zend_compile_file = em_compile_file;

    return SUCCESS;
}

bool EMSCRIPTEN_KEEPALIVE em_env_import(const char* env, size_t elen) {
    if (zend_hash_num_elements(&__em_environ__)) {
        /**
         * We need every import to clear the table
         */
        zend_hash_clean(&__em_environ__);
    }

    return em_dispatch_env(&__em_environ__, env, elen, true);
}

uintptr_t EMSCRIPTEN_KEEPALIVE em_run_string(const char* code, size_t length) {
    em_dispatch_context_t* context =
        em_dispatch_enter_code(
            &SG(request_info), code, length);

    if (em_activate(false) != SUCCESS) {
        return (uintptr_t) -1;
    }

    context->handler(context);

    em_deactivate();

    return (uintptr_t) em_dispatch_leave(context);
}

uintptr_t EMSCRIPTEN_KEEPALIVE em_run_script(const char* script) {
    em_dispatch_context_t* context =
        em_dispatch_enter_script(
            &SG(request_info), script);

    if (em_activate(false) != SUCCESS) {
        return (uintptr_t) -1;
    }

    context->handler(context);

    em_deactivate();

    return (uintptr_t) em_dispatch_leave(context);
}

uintptr_t EMSCRIPTEN_KEEPALIVE em_run_request(
    const char* env,  size_t elen,
    const char* head, size_t hlen,
    const char* body, size_t blen) {
    em_dispatch_context_t* context =
        em_dispatch_enter(
            &SG(request_info),
            env,  elen,
            head, hlen,
            body, blen);

    if (em_activate(true) != SUCCESS) {
        return (uintptr_t) -1;
    }

    context->handler(context);

    em_deactivate();

    return (uintptr_t) em_dispatch_leave(context);
}

char* EMSCRIPTEN_KEEPALIVE em_run_result(uintptr_t address) {
    em_dispatch_context_t* context =
        (em_dispatch_context_t*) address;
    if (!context) {
        return NULL;
    }
    return context->buffers.response.join.value;
}

size_t EMSCRIPTEN_KEEPALIVE em_run_length(uintptr_t address) {
    em_dispatch_context_t* context =
        (em_dispatch_context_t*) address;
    if (!context) {
        return 0;
    }
    return context->buffers.response.join.length;
}

void EMSCRIPTEN_KEEPALIVE em_run_free(uintptr_t address) {
    em_dispatch_context_t* context =
        (em_dispatch_context_t*) address;
    if (!context) {
        return;
    }

    em_dispatch_free(context);
}

void EMSCRIPTEN_KEEPALIVE em_shutdown(void) {
    em_stdxx_shutdown();
    em_dispatch_shutdown();
    em_http_shutdown();
    em_vfs_shutdown();

    sapi_module.ub_write = zend_write_func;
    sapi_module.log_message = zend_log_func;

    php_module_shutdown();

    sapi_shutdown();

#ifdef ZTS
    tsrm_shutdown();
#endif

#ifdef HAVE_EM_SHM
    em_shm_shutdown();
#endif
} /* }}} */

/* {{{ sapi gubbins */
static int em_sapi_startup(sapi_module_struct *sapi_module)
{
#if PHP_VERSION_ID < 80200
    return php_module_startup(sapi_module, NULL, 0);
#else
    return php_module_startup(sapi_module, NULL);
#endif
}

size_t em_sapi_write(const char* buf, size_t len) {
	const char *ptr = buf;
	size_t remaining = len;
	size_t ret;

	while (remaining > 0) {
        ret = fwrite(ptr, 1, MIN(remaining, 16384), stdout);
		if (!ret) {
			php_handle_aborted_connection();
		}
		ptr += ret;
		remaining -= ret;
	}

	return ret;
}

void em_sapi_error(int type, const char *message, ...) {
    va_list args;
    va_start(args, message);
    vfprintf(
        stderr,
        message,
        args);
    va_end(args);
}

void em_sapi_log(const char* message, int type) {
    fprintf(
        stderr,
        "em internal error: %s",
        message);
}

static void em_sapi_flush(void* ctx) {
    (void) ctx;

    if (fflush(stdout) == EOF ||
        fflush(stderr) == EOF) {
            php_handle_aborted_connection();
    }
}

static char* em_sapi_cookies(void)
{
	return NULL;
}

static size_t em_sapi_post(char* buffer, size_t length) {
    em_dispatch_context_t* context = SG(server_context);

    if (!context->buffers.request.body.length) {
        /* nothing buffered */
        return 0;
    }

    if (context->buffers.request.body.position ==
        context->buffers.request.body.length) {
        /* empty buffer */
        return 0;
    }

    /* grab a chunk of buffer */
    size_t chunk =
        length > (context->buffers.request.body.length -
                  context->buffers.request.body.position) ?
            (context->buffers.request.body.length -
             context->buffers.request.body.position) :
        length;

    memcpy(buffer,
        context->buffers.request.body.value +
        context->buffers.request.body.position,
        chunk);

    context->buffers.request.body.position += chunk;

    return chunk;
}

sapi_header_struct* em_sapi_headers_status(sapi_headers_struct* all, zend_llist_position* position) {
    sapi_header_struct* one = zend_llist_get_first_ex(&all->headers, position);

    do {
        if (!one) {
            break;
        }

        if ((one->header_len > sizeof("Status:")-1) &&
            (strncasecmp(one->header,
                "Status:", sizeof("Status:")-1) == SUCCESS)) {
            return one;
        }
    } while ((one = zend_llist_get_next_ex(&all->headers, position)));

    return NULL;
}

static int em_sapi_headers(sapi_headers_struct* all) {
    if (SG(request_info).no_headers) {
        SG(headers_sent) = 1;

        return SAPI_HEADER_SENT_SUCCESSFULLY;
    }

    zend_llist_position position;
    sapi_header_struct* status =
        em_sapi_headers_status(all, &position);

    /* send status line first */
    if (status) {
        const char* http =
            strchr(status->header, ':');

        if (http) {
            em_dispatch_response(SG(server_context), EM_DISPATCH_HEAD, ZEND_STRL("HTTP/1.0"));
            em_dispatch_response(SG(server_context), EM_DISPATCH_HEAD, http + 1, strlen(http + 1));
        } else {
            em_dispatch_response(SG(server_context), EM_DISPATCH_HEAD, status->header, status->header_len);
        }

        em_dispatch_response(SG(server_context), EM_DISPATCH_HEAD, ZEND_STRL("\r\n"));
    } else {
        char buffer[4096];
        snprintf(buffer, 4096,
            "HTTP/1.0 %d %s",
            all->http_response_code, all->http_status_line);
        em_dispatch_response(SG(server_context), EM_DISPATCH_HEAD, buffer, strlen(buffer));
        em_dispatch_response(SG(server_context), EM_DISPATCH_HEAD, ZEND_STRL("\r\n"));
    }
    
    /* send remainder of headers */
    sapi_header_struct* next = zend_llist_get_first_ex(&all->headers, &position);
    do {
        if (!next) {
            goto __em_sapi_headers_leave;
        }

        if (next == status) {
            continue;
        }

        em_dispatch_response(SG(server_context), EM_DISPATCH_HEAD, next->header, next->header_len);
        em_dispatch_response(SG(server_context), EM_DISPATCH_HEAD, ZEND_STRL("\r\n"));
    } while ((next = zend_llist_get_next_ex(&all->headers, &position)));

__em_sapi_headers_leave:
    em_dispatch_response(SG(server_context), EM_DISPATCH_HEAD, ZEND_STRL("\r\n"));

    return SAPI_HEADER_SENT_SUCCESSFULLY;
}

static char* em_sapi_getenv(const char* name, size_t length) {
    em_dispatch_context_t* context = SG(server_context);
    HashTable* table = context ?
        &context->environ :
        &__em_environ__;

    zval* item = zend_hash_str_find(
        table, name, length);

    if (!item) {
        return NULL;
    }

    return Z_STRVAL_P(item);
}

static void em_sapi_server(zval *vars)
{
    em_dispatch_context_t* context = SG(server_context);

	php_import_environment_variables(vars);

    if (SG(request_info).request_method) {
        php_register_variable("REQUEST_METHOD",
            (char*)SG(request_info).request_method, vars);
    }

    if (SG(request_info).request_uri) {
        php_register_variable("REQUEST_URI",
            (char*)SG(request_info).request_uri, vars);
    }

    if (SG(request_info).path_translated) {
        php_register_variable("SCRIPT_FILENAME",
            (char*)SG(request_info).path_translated, vars);
        php_register_variable("PHP_SELF",
            (char*)SG(request_info).path_translated, vars);
    }

    zend_llist_position position;
    em_dispatch_header_t* header =
        zend_llist_get_first_ex(
            &context->headers.request, &position);
    if (header && (header = zend_llist_get_next_ex(&context->headers.request, &position))) {
        do {
            /* CGI standard:
                prefix with HTTP_
                replace - with _
                uppercase characters */
            char* skey = emalloc(
                header->key.len + sizeof("HTTP_"));
            memcpy(skey,
                ZEND_STRL("HTTP_"));

            for (size_t pos = 0; pos < header->key.len; pos++) {
                switch (header->key.data[pos]) {
                    case '-':
                        skey[(sizeof("HTTP_") -1) + pos] = '_';
                    break;

                    default:
                        skey[(sizeof("HTTP_") -1) + pos] =
                            toupper(header->key.data[pos]);
                }
            }

            skey[(sizeof("HTTP_") -1) + header->key.len] = 0;

            php_register_variable(
                skey, header->value.data, vars);
            efree(skey);
        } while ((header = zend_llist_get_next_ex(&context->headers.request, &position)));
    }
}

sapi_module_struct em_sapi_module = {
    "em",                         /* name */
    "em",                         /* pretty name */
    em_sapi_startup,              /* startup */
    php_module_shutdown_wrapper,  /* shutdown */
    NULL,                         /* activate */
    NULL,                         /* deactivate */
    em_sapi_write,                /* ub write */
    em_sapi_flush,                /* flush */
    NULL,                         /* get uid */
    em_sapi_getenv,               /* getenv */
    em_sapi_error,                /* error handler */
    NULL,                         /* header handler */
    em_sapi_headers,              /* send headers handler */
    NULL,                         /* send header handler */
    em_sapi_post,                 /* read post */
    em_sapi_cookies,              /* read cookies */
    em_sapi_server,               /* register server variables */
    em_sapi_log,                  /* log message */
    NULL,                         /* get request time */
    NULL,                         /* terminate process */
    STANDARD_SAPI_MODULE_PROPERTIES
};
/* }}} */