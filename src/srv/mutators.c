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

static pcre2_code* __em_mutators_tags_pattern__;
static pcre2_code* __em_mutators_attribute_pattern__;

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

    pcre2_match_data *tags =
        pcre2_match_data_create_from_pattern(
            __em_mutators_tags_pattern__, NULL);
    pcre2_match_data *attriubute =
        pcre2_match_data_create_from_pattern(
            __em_mutators_attribute_pattern__, NULL);

    if (!tags || !attriubute) {
        if (tags)
            pcre2_match_data_free(tags);
        if (attriubute)
            pcre2_match_data_free(attriubute);
        return false;
    }

    smart_str buffer = {0};
    size_t tlast = 0;
    PCRE2_SIZE toffset = 0;
    int trc, arc;

    em_buffer_t* body = &context->buffers.response.body;

    while ((trc = pcre2_match(__em_mutators_tags_pattern__,
            (PCRE2_SPTR)body->value, (PCRE2_SIZE)body->length,
            toffset, 0, tags, NULL)) >= 0) {

        PCRE2_SIZE *tovector = pcre2_get_ovector_pointer(tags);

        // Copy content before this tag
        smart_str_appendl(
            &buffer,
            body->value + tlast,
            tovector[0] - tlast);

        // Copy the whole tag to a temporary buffer
        size_t tlength = tovector[1] - tovector[0];
        char *tbuffer  = emalloc(tlength + 1);
        memcpy(
            tbuffer, 
            body->value + tovector[0],
            tlength);
        tbuffer[tlength] = '\0';

        // Find attributes in the tag buffer and mutate them
        size_t aoffset = 0, alast = 0;
        while ((arc = pcre2_match(__em_mutators_attribute_pattern__,
                (PCRE2_SPTR)tbuffer, (PCRE2_SIZE)tlength,
                aoffset, 0, attriubute, NULL)) >= 0) {

            PCRE2_SIZE *aovector = pcre2_get_ovector_pointer(attriubute);

            // Copy content before this attribute
            smart_str_appendl(
                &buffer,
                tbuffer + alast,
                aovector[0] - alast);

            size_t alength = aovector[5] - aovector[4];

            char aname[32];
            snprintf(aname, sizeof(aname),
                "%.*s",
                (int)(aovector[3] - aovector[2]),
                tbuffer + aovector[2]);
            char *avalue = emalloc(alength + 1);
            memcpy(
                avalue,
                tbuffer + aovector[4],
                alength);
            avalue[alength] = '\0';

            if (strcasecmp(aname, "href")   == 0 ||
                strcasecmp(aname, "src")    == 0 ||
                strcasecmp(aname, "action") == 0) {
                char *mutated = em_mutators_href(context, avalue, alength);
                if (mutated) {
                    smart_str_appends(
                        &buffer, aname);
                    smart_str_appends(
                        &buffer, "=\"");
                    smart_str_appends(
                        &buffer, mutated);
                    smart_str_appends(
                        &buffer, "\"");
                    efree(mutated);
                } else {
                    smart_str_appendl(
                        &buffer,
                        tbuffer + aovector[0],
                        aovector[1] - aovector[0]);
                }
                efree(avalue);
            } else {
                smart_str_appendl(
                    &buffer, 
                    tbuffer + aovector[0],
                    aovector[1] - aovector[0]);
                efree(avalue);
            }

            alast = aovector[1];
            aoffset = aovector[1];
        }

        // Copy remaining part of the tag after last attribute
        if (alast < tlength) {
            smart_str_appendl(
                &buffer,
                tbuffer + alast,
                tlength - alast);
        }
        efree(tbuffer);

        tlast   = tovector[1];
        toffset = tovector[1];
    }

    // Copy remaining content after last tag
    if (tlast < body->length) {
        smart_str_appendl(
            &buffer,
            body->value + tlast,
            body->length - tlast);
    }

    smart_str_0(&buffer);

    // Replace output
    if (buffer.s) {
        em_buffer_clear(&context->buffers.response.body, true);
        em_buffer_write(&context->buffers.response.body,
            ZSTR_VAL(buffer.s),
            ZSTR_LEN(buffer.s));
        smart_str_free(&buffer);
    }

    pcre2_match_data_free(tags);
    pcre2_match_data_free(attriubute);

    if (em_mutators_sizeof(context)) {
        em_mutators_length(context);
    }

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
    PCRE2_SPTR tags = (PCRE2_SPTR)
        "<\\s*"
        "([a-zA-Z0-9\\-]+)"  /* Group 1: Tag Name*/
        "([^>]+?)\\s*\\/?>"; /* Group 2: Attributes */

    PCRE2_SPTR attribute = (PCRE2_SPTR)
        "(href|src|action|style)" /* Group 1: Attribute Name */
        "\\s*="                   
        "\\s*[\"']?"
        "(?!data:|\\/\\/)"        
        "([^\"'>\\s]+)"           /* Group 2: Attribute Value */
        "[\"']?";

    int code;
    PCRE2_SIZE offset;

    __em_mutators_tags_pattern__ =
        pcre2_compile(
            tags, strlen((char*)tags),
            PCRE2_CASELESS | PCRE2_DOTALL,
            &code, &offset,
            NULL);

    if (!__em_mutators_tags_pattern__) {
        fprintf(stderr,
            "[mutators] failed to compile "
            "__em_mutators_tags_pattern__ %d at %zu\n",
            code, offset);
    }

    __em_mutators_attribute_pattern__ =
        pcre2_compile(
            attribute, strlen((char*)attribute),
            PCRE2_CASELESS | PCRE2_DOTALL,
            &code, &offset,
            NULL);

    if (!__em_mutators_attribute_pattern__) {
        fprintf(stderr,
            "[mutators] failed to compile "
            "__em_mutators_attribute_pattern__ %d at %zu\n",
            code, offset);
    }
}

void em_mutators_activate(void) {
    
}

void em_mutators_deactivate(void) {
    
}

void em_mutators_shutdown(void) {
    if (__em_mutators_attribute_pattern__) {
        pcre2_code_free(
            __em_mutators_attribute_pattern__);
    }
}