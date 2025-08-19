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

#include <emscripten.h>
#include <emscripten/eventloop.h>

#include <SAPI.h>

#include "proc.h"

typedef struct _em_proc_t em_proc_t;

typedef int (*em_proc_entry_t)(em_proc_t* proc);

struct _em_proc_t {
    int   pid;
    int   status;
    bool  running;
    bool  finished;
    char* command;
    em_proc_entry_t entry;
    int    argc;
    char** argv;
    zval   pipes;
};

static int em_proc_le;

/**
 * I have done much experimentation to discover if we can support proc_open.
 * 
 * emscripten_fiber_t:
 *   These are not prepared to swap the vm context, and for some reason don't
 *   play nicely with emscripten_sleep (used at io layer).
 *   It would seem like a solution to merge the PHP definition of Fiber and the
 *   emscripten definition, such that we could run processes in a PHP Fiber which
 *   relies on emscriptens fiber context swapping mechanism; However, it's that
 *   mechanism which looks broken when mixed with emscripten_sleep from deep
 *   in the stack.
 * wasm workers:
 *   These seem like a really nice option, unfortunately, wasm workers are only
 *   enabled if the server sends specific headers, or the browser is started
 *   with specific options ... this is crazy, the server has no stake in what
 *   goes on in the browser ... but they are the rules, so whatever, it's not
 *   widely usable.
 * 
 * For now, we're going to fake it until we make it ... the current code
 * will allow an application to continue past proc_open, but is obviously terribly
 * useless for anything else
 * 
 * !This code is unfinished and messy, don't use proc open if you can help it!
 */

int em_proc_entry(em_proc_t* proc) {
    int count = 0;

    /**
     * This routine will be entered async, and could be used
     * to execute anything that is not going to call emscripten_sleep
     * or block for a long time ...
     * Unfortunately, because of the nature of ASYNCIFY we have no idea
     * what may yield to the browser ...
     * My original idea was to compile tools like git into this same
     * address space and delegate to their main() functions from here
     * with argc,argv from proc_open ...
     * Problematic is redirecting streams ...
     * I don't have all the answers ...
     */
    php_stream* out;
    php_stream_from_zval_no_verify(out,
        zend_hash_index_find(Z_ARRVAL(proc->pipes), 1));

    while (count++ < 1000) {
        php_stream_write(out,
            "doing things in the proceess\n",
            sizeof("doing things in the proceess\n")-1);
    }

    php_stream_seek(out, 0, SEEK_SET);
    return 0;
}

void em_proc_run(void* arg) {
    em_proc_t* proc =
        (em_proc_t*) arg;

    proc->status =
        proc->entry(proc);
    proc->running = false;
    proc->finished = true;
}

static void em_proc_dtor(zend_resource *rsrc)
{
    em_proc_t *proc = (em_proc_t *)rsrc->ptr;

    while (!proc->finished) {
        emscripten_sleep(100);
    }

    zval_ptr_dtor(&proc->pipes);

    if (proc->command) {
        efree(proc->command);
    }
    efree(proc);
}

ZEND_BEGIN_ARG_INFO_EX(em_proc_open_arginfo, 0, 0, 3)
    ZEND_ARG_INFO(0, command)
    ZEND_ARG_INFO(0, descriptorspec)
    ZEND_ARG_INFO(1, pipes)
    ZEND_ARG_INFO(0, cwd)
    ZEND_ARG_INFO(0, env)
    ZEND_ARG_INFO(0, other_options)
ZEND_END_ARG_INFO()

PHP_NAMED_FUNCTION(em_proc_open)
{
    zend_string *command;
    zval *descriptorspec,
         *pipes,
         *cwd = NULL,
         *env = NULL,
         *other_options = NULL;
    
    ZEND_PARSE_PARAMETERS_START(3, 6)
        Z_PARAM_STR(command)
        Z_PARAM_ARRAY(descriptorspec)
        Z_PARAM_ZVAL(pipes)
        Z_PARAM_OPTIONAL
        Z_PARAM_ZVAL_OR_NULL(cwd)
        Z_PARAM_ZVAL_OR_NULL(env)
        Z_PARAM_ZVAL_OR_NULL(other_options)
    ZEND_PARSE_PARAMETERS_END();

    em_proc_t *proc = emalloc(sizeof(em_proc_t));
    proc->pid = rand() % 1000 + 1000; // Fake PID
    proc->status = 0;
    proc->running = true;
    proc->finished = false;
    proc->command = estrdup(ZSTR_VAL(command));
    proc->entry = em_proc_entry;

    pipes = zend_try_array_init(pipes);

    ZVAL_COPY(
        &proc->pipes, pipes);

    // Create fake streams for descriptors
    HashTable *desc_ht = Z_ARRVAL_P(descriptorspec);
    zval *desc_entry;
    zend_string* desc_key;
    zend_ulong desc_num = 0;
    
    ZEND_HASH_FOREACH_KEY_VAL(desc_ht, desc_num, desc_key, desc_entry) {
        ZVAL_DEREF(desc_entry);
        if (Z_TYPE_P(desc_entry) == IS_ARRAY) {
            zval *type = zend_hash_index_find(Z_ARRVAL_P(desc_entry), 0);
            if (type && Z_TYPE_P(type) == IS_STRING) {
                php_stream *stream = NULL;
                
                if (strcmp(Z_STRVAL_P(type), "pipe") == 0) {
                    // Create fake pipe stream
                    stream = php_stream_memory_create(TEMP_STREAM_DEFAULT);
                } else if (strcmp(Z_STRVAL_P(type), "file") == 0) {
                    // Handle file descriptor
                    stream = php_stream_memory_create(TEMP_STREAM_DEFAULT);
                }
                
                if (stream) {
                    zval stream_zval;
                    php_stream_to_zval(stream, &stream_zval);
                    add_index_zval(
                        pipes, desc_num, &stream_zval);
                }
            }
        }
    } ZEND_HASH_FOREACH_END();

    emscripten_set_timeout(
        em_proc_run, 0, proc);
    emscripten_sleep(20); // Give up a slice immediately
    
    RETURN_RES(zend_register_resource(proc, em_proc_le));
}

ZEND_BEGIN_ARG_INFO_EX(em_proc_status_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, process)
ZEND_END_ARG_INFO()

PHP_NAMED_FUNCTION(em_proc_status)
{
    zval* res;
    em_proc_t* proc;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_RESOURCE(res)
    ZEND_PARSE_PARAMETERS_END();

    if ((proc = (em_proc_t *)zend_fetch_resource(
            Z_RES_P(res),
            "process", em_proc_le)) == NULL) {
        RETURN_FALSE;
    }

    if (!proc->running &&
        !proc->finished) {
        /**
         * Give up a slice if it's still required
         */
        emscripten_sleep(20);
    }

    array_init(return_value);
    add_assoc_str(
        return_value,
        "command",
        zend_string_init(
            proc->command, strlen(proc->command), 0
    ));
    add_assoc_long(return_value, "pid", proc->pid);
  	add_assoc_bool(return_value, "running", proc->running);
  	add_assoc_bool(return_value, "signaled", false);
  	add_assoc_bool(return_value, "stopped", false);
  	add_assoc_long(return_value, "exitcode", proc->status);
  	add_assoc_long(return_value, "termsig", 0);
  	add_assoc_long(return_value, "stopsig", 0);
}

ZEND_BEGIN_ARG_INFO_EX(em_proc_close_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, process)
ZEND_END_ARG_INFO()

PHP_NAMED_FUNCTION(em_proc_close)
{
    zval *res;
    em_proc_t *proc;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_RESOURCE(res)
    ZEND_PARSE_PARAMETERS_END();

    if ((proc = (em_proc_t *)zend_fetch_resource(
            Z_RES_P(res),
            "process", em_proc_le)) == NULL) {
        RETURN_FALSE;
    }

    RETVAL_LONG(proc->status);
    zend_list_close(
        Z_RES_P(res));
}

zend_function_entry em_proc_functions[] = {
    PHP_NAMED_FE(proc_open,       em_proc_open,   em_proc_open_arginfo)
    PHP_NAMED_FE(proc_get_status, em_proc_status, em_proc_status_arginfo)
    PHP_NAMED_FE(proc_close,      em_proc_close,  em_proc_close_arginfo)

    PHP_FE_END
};

void em_proc_startup(void) {
    em_proc_le = zend_register_list_destructors_ex(
        em_proc_dtor,
        NULL, "process",
        12);

    zend_hash_str_del(CG(function_table), ZEND_STRL("proc_open"));
    zend_hash_str_del(CG(function_table), ZEND_STRL("proc_get_status"));
    zend_hash_str_del(CG(function_table), ZEND_STRL("proc_close"));

    zend_register_functions(NULL,
        em_proc_functions,
        CG(function_table),
        MODULE_PERSISTENT);
}

void em_proc_shutdown(void) {
    zend_unregister_functions(
        em_proc_functions,
        sizeof(em_proc_functions)/sizeof(zend_function_entry),
        CG(function_table));
}