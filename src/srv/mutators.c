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

#include <srv/mutators.h>
#include <buffer/buffer.h>

#include <pcre2.h>

#include <zend_smart_str.h>

static pcre2_code* __em_mutators_html_pattern__;

char* em_mutators_header(em_dispatch_context_t* context, const char* search) {
    zend_llist_position position;
    sapi_header_struct* find = zend_llist_get_first_ex(&SG(sapi_headers).headers, &position);

    if (!find) {
        return NULL;
    }

    do {
        if (strcasestr(find->header, search)) {
            char* start = strchr(find->header, ':');

            if (!start) {
                return NULL;
            }
            start++;

            while (isspace(*start)) {
                start++;
            }

            return start;
        }
    } while ((find = zend_llist_get_next_ex(&SG(sapi_headers).headers, &position)));

    return NULL;
}

char* em_mutators_location(em_dispatch_context_t* context) {
    return em_mutators_header(context, "location");
}

char* em_mutators_typeof(em_dispatch_context_t* context) {
    return em_mutators_header(context, "content-type");
}

char* em_mutators_sizeof(em_dispatch_context_t* context) {
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

    em_url_t destination;
    em_url_parse(context, href, &destination);
    if (!em_url_compare(
            EM_URL_COMPARE_ORIGIN,
            &context->url,
            &destination)) {
        em_url_free(&destination);
        return NULL;
    }

    char* rewritten = em_url_string(
        EM_URL_FQU, &destination);
    em_url_free(&destination);
    return rewritten;
}

static bool em_mutators_html(em_dispatch_context_t* context) {
    if (!context->buffers.response.body.value) {
        return false;
    }

    pcre2_match_data *matches =
        pcre2_match_data_create_from_pattern(
            __em_mutators_html_pattern__, NULL);
    if (!matches) {
        return false;
    }

    smart_str new_content = {0};
    size_t last_pos = 0;
    PCRE2_SIZE offset = 0;
    int rc;

    while ((rc = pcre2_match(__em_mutators_html_pattern__,
            (PCRE2_SPTR)
                context->buffers.response.body.value,
            (PCRE2_SIZE)
                context->buffers.response.body.length,
            offset, 0, matches, NULL)) >= 0) {
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(matches);
        // ovector[0] = start of full match, ovector[1] = end of full match
        // ovector[2] = start of group 1, ovector[3] = end of group 1
        // ovector[4] = start of group 2, ovector[5] = end of group 2

        // Copy content from last_pos to start of match
        smart_str_appendl(&new_content,
            context->buffers.response.body.value + last_pos,
            ovector[0] - last_pos);

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

        if (strcasecmp(attr_name, "href")   == 0 ||
            strcasecmp(attr_name, "src")    == 0 ||
            strcasecmp(attr_name, "action") == 0) {
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
    if (last_pos < context->buffers.response.body.length) {
        smart_str_appendl(&new_content,
            context->buffers.response.body.value  + last_pos,
            context->buffers.response.body.length - last_pos);
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

    pcre2_match_data_free(matches);

    return true;
}

static bool em_mutators_css(em_dispatch_context_t* context) {
    return false;
}

static void em_mutators_redirect(
    em_dispatch_context_t* context,
    char* location) {
    em_url_t redirect;

    if (!em_url_parse(context, location, &redirect)) {
        return;
    }

    if (!em_url_compare(
            EM_URL_COMPARE_ORIGIN,
            &context->url, &redirect)) {
        em_url_free(&redirect);
        return;
    }

    char* value = em_url_string(EM_URL_REL, &redirect);

    if (value) {
        em_dispatch_header(context,
            "Location: %s", value);
        free(value);
    }

    em_url_free(&redirect);
}

static void em_mutators_headers(em_dispatch_context_t* context) {
    char* location =
        em_mutators_location(context);
    if (location) {
        em_mutators_redirect(context, location);
    }
}

bool em_mutators_mutate(em_dispatch_context_t* context) {
    em_mutators_headers(context);

    char* type = em_mutators_typeof(context);

    if (!type) {
        return false;
    }

    if (strcasestr(type, "text/html")) {
        return em_mutators_html(context);
    } else if (strcasestr(type, "text/css")) {
        return em_mutators_css(context);
    }

    return false;
}

void em_mutators_startup(void) {
    PCRE2_SPTR href = (PCRE2_SPTR)
        "(href|src|action|style)"
        "\\s*="
        "\\s*[\"']?"
        "(?!data:|\\/\\/)"
        "([^\"'>\\s]+)"
        "[\"']?";

    int code;
    PCRE2_SIZE offset;

    __em_mutators_html_pattern__ =
        pcre2_compile(
            href, strlen((char*)href),
            PCRE2_CASELESS | PCRE2_DOTALL,
            &code, &offset,
            NULL);

    if (!__em_mutators_html_pattern__) {
        fprintf(stderr,
            "[mutators] failed to compile "
            "__em_mutators_html_pattern__ %d at %zu\n",
            code, offset);
    }
}

void em_mutators_activate(void) {
    
}

void em_mutators_deactivate(void) {
    
}

void em_mutators_shutdown(void) {
    if (__em_mutators_html_pattern__) {
        pcre2_code_free(
            __em_mutators_html_pattern__);
    }
}