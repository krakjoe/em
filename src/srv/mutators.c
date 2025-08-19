/*
  +----------------------------------------------------------------------+
  | em - Clean Parser Implementation                                     |
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

#include "mutators.h"

#include <pcre2.h>

#include <zend_smart_str.h>

sapi_header_struct* em_mutators_header(em_dispatch_context_t* context, const char* search) {
    zend_llist_position position;
    sapi_header_struct* find = zend_llist_get_first_ex(&SG(sapi_headers).headers, &position);

    if (!find) {
        return NULL;
    }

    do {
        if (strcasestr(find->header, search)) {
            return find;
        }
    } while ((find = zend_llist_get_next_ex(&SG(sapi_headers).headers, &position)));

    return NULL;
}

sapi_header_struct* em_mutators_typeof(em_dispatch_context_t* context) {
    return em_mutators_header(context, "content-type");
}

sapi_header_struct* em_mutators_sizeof(em_dispatch_context_t* context) {
    return em_mutators_header(context, "content-length");
}

static void em_mutators_length(em_dispatch_context_t* context) {
    em_dispatch_header(context, "Content-Length: %d",
        context->buffers.response.body.length);
}

static char* em_mutators_href(em_dispatch_context_t* context, const char* href, size_t length) {
    zval* vroot = zend_hash_str_find(
        &context->environ, ZEND_STRL("VIRTUAL_ROOT"));

    if (!vroot) {
        return NULL;
    }

    // Do not mutate external URLs
    if ((length >= 7 && strncasecmp(href, "http://", 7) == 0) ||
        (length >= 8 && strncasecmp(href, "https://", 8) == 0) ||
        (length >= 2 && href[0] == '/' && href[1] == '/')) {
        return NULL;
    }

    // Do not mutate if already starts with VIRTUAL_ROOT
    if (length >= Z_STRLEN_P(vroot) && 
        strncmp(href,
            Z_STRVAL_P(vroot),
            Z_STRLEN_P(vroot)) == SUCCESS) {
        return NULL;
    }

    // Remove leading slash from href if present
    size_t hoffset = 0;
    if (length > 0 && href[0] == '/') {
        hoffset = 1;
        length -= 1;
    }

    // Add slash between vroot and href if needed
    size_t rlength;
    char* rewritten;
    if (Z_STRLEN_P(vroot) > 0 && Z_STRVAL_P(vroot)[Z_STRLEN_P(vroot)-1] == '/') {
        rlength = Z_STRLEN_P(vroot) + length;
        rewritten = emalloc(rlength + 1);
        memcpy(rewritten,
            Z_STRVAL_P(vroot),
            Z_STRLEN_P(vroot));
        memcpy(rewritten + Z_STRLEN_P(vroot),
            href + hoffset,
            length);
    } else {
        rlength = Z_STRLEN_P(vroot) + 1 + length;
        rewritten = emalloc(rlength + 1);
        memcpy(rewritten,
            Z_STRVAL_P(vroot),
            Z_STRLEN_P(vroot));
        rewritten[Z_STRLEN_P(vroot)] = '/';
        memcpy(rewritten + Z_STRLEN_P(vroot) + 1,
            href + hoffset,
            length);
    }
    rewritten[rlength] = '\0';
    return rewritten;
}

static bool em_mutators_html(em_dispatch_context_t* context) {
    if (!context->buffers.response.body.value) {
        return false;
    }

    const char *pattern = 
        "\\b(href|src|action|style)\\s*=\\s*[\"']?(?!data:)([^\"'>\\s]+)[\"']?";
    size_t pattern_len = strlen(pattern);

    PCRE2_SIZE subject_len = context->buffers.response.body.length;
    PCRE2_SPTR subject = (PCRE2_SPTR)
        context->buffers.response.body.value;

    int errorcode;
    PCRE2_SIZE erroroffset;
    uint32_t options = PCRE2_CASELESS | PCRE2_DOTALL;

    pcre2_code *re = pcre2_compile(
        (PCRE2_SPTR)pattern, pattern_len, options, &errorcode, &erroroffset, NULL);
    if (!re) {
        return false;
    }

    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, NULL);
    if (!match_data) {
        pcre2_code_free(re);
        return false;
    }

    smart_str new_content = {0};
    size_t last_pos = 0;
    PCRE2_SIZE offset = 0;
    int rc;

    while ((rc = pcre2_match(re, subject, subject_len, offset, 0, match_data, NULL)) >= 0) {
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
        // ovector[0] = start of full match, ovector[1] = end of full match
        // ovector[2] = start of group 1, ovector[3] = end of group 1
        // ovector[4] = start of group 2, ovector[5] = end of group 2

        // Copy content from last_pos to start of match
        smart_str_appendl(&new_content,
            context->buffers.response.body.value + last_pos,
            ovector[0] - last_pos);

        // Extract attribute name and value
        size_t value_len = ovector[5] - ovector[4];

        // This is nasty, wasm doesn't like locals ...
        char attr_name[32];
        snprintf(attr_name, sizeof(attr_name), "%.*s",
            (int)(ovector[3] - ovector[2]),
            context->buffers.response.body.value + ovector[2]);
        char *attr_value = emalloc(ovector[5] - ovector[4] + 1);
        memcpy(attr_value,
            context->buffers.response.body.value + ovector[4],
            ovector[5] - ovector[4]);
        attr_value[ovector[5] - ovector[4]] = '\0';

        if (strcasecmp(attr_name, "href") == 0 || strcasecmp(attr_name, "src") == 0) {
            char *mutated = em_mutators_href(context, attr_value, ovector[5] - ovector[4]);
            if (mutated) {
                smart_str_appends(&new_content, attr_name);
                smart_str_appends(&new_content, "=\"");
                smart_str_appends(&new_content, mutated);
                smart_str_appends(&new_content, "\"");
                efree(mutated);
            } else {
                smart_str_appendl(&new_content,
                    context->buffers.response.body.value + ovector[0],
                    ovector[1] - ovector[0]);
            }
            efree(attr_value);
        } else {
            smart_str_appendl(&new_content,
                context->buffers.response.body.value + ovector[0],
                ovector[1] - ovector[0]);
            efree(attr_value);
        }

        last_pos = ovector[1];
        offset = ovector[1];
    }

    // Copy remaining content after last match
    if (last_pos < subject_len) {
        smart_str_appendl(&new_content,
            context->buffers.response.body.value + last_pos,
            subject_len - last_pos);
    }

    smart_str_0(&new_content);

    // Replace output
    if (new_content.s) {
        em_buffer_clear(&context->buffers.response.body, true);
        em_buffer_write(&context->buffers.response.body,
            ZSTR_VAL(new_content.s),
            ZSTR_LEN(new_content.s));
        smart_str_free(&new_content);
    }

    if (em_mutators_sizeof(context)) {
        em_mutators_length(context);
    }

    pcre2_match_data_free(match_data);
    pcre2_code_free(re);

    return true;
}

static bool em_mutators_css(em_dispatch_context_t* context) {
    return false;
}

bool em_mutators_mutate(em_dispatch_context_t* context) {
    sapi_header_struct* type = em_mutators_typeof(context);

    if (!type) {
        return false;
    }

    if (strcasestr(type->header, "text/html")) {
        return em_mutators_html(context);
    } else if (strcasestr(type->header, "text/css")) {
        return em_mutators_css(context);
    }

    return false;
}

void em_mutators_activate(void) {
    
}

void em_mutators_deactivate(void) {
    
}