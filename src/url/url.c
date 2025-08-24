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

#include <SAPI.h>
#include <pcre2.h>
#include <libgen.h>

#include <url/url.h>
#include <srv/dispatch.h>

static pcre2_code* __em_url_pattern_fqu__; /* fully qualified uri */
static pcre2_code* __em_url_pattern_rel__; /* relative uri */

#define __PRE2_EXTRACT__(ovector, offset, idx, field) do {      \
    if (ovector[offset*(idx)]   != PCRE2_UNSET &&               \
        ovector[offset*(idx)+1] != PCRE2_UNSET) {               \
        size_t len = ovector[offset*(idx)+1] -                  \
                        ovector[offset*(idx)];                  \
        if (len > 0) {                                          \
            parsed->field = strndup(                            \
                (const char*)url + ovector[offset*(idx)], len); \
        } else {                                                \
            parsed->field = NULL;                               \
        }                                                       \
    } else {                                                    \
        parsed->field = NULL;                                   \
    }                                                           \
} while(0)

#if 0
static inline void em_url_debug(const char* url, em_url_t* parsed) {
    fprintf(stderr,
        "[url] em_url_parse(%s) = %s:\n"
        "\tScheme:   %s\n"
        "\tHost:     %s\n"
        "\tPort:     %u\n"
        "\tURI:      %s\n"
        "\tPath:     %s -> %s\n"
        "\tQuery:    %s\n"
        "\tFragment: %s\n",
        url, (parsed->kind == EM_URL_FQU) ?
            "FQU" : "REL",
        parsed->scheme.raw ? parsed->scheme.raw : "(null)",
        parsed->host       ? parsed->host :       "(null)",
        parsed->port.number,
        parsed->uri        ? parsed->uri :        "(null)",
        parsed->path.web   ? parsed->path.web :   "(null)",
        parsed->path.vfs   ? parsed->path.vfs :   "(null)",
        parsed->query      ? parsed->query :      "(null)",
        parsed->fragment   ? parsed->fragment :   "(null)");
}
#endif

static char* em_url_parse_uri(em_url_t* parsed) {
    char* uri = malloc(
        (parsed->path.web ? strlen(parsed->path.web)     : 0) +
        (parsed->query    ? strlen(parsed->query)    + 1 : 0) +
        (parsed->fragment ? strlen(parsed->fragment) + 1 : 0) +
        1
    );

    uri[0] = '\0';

    if (parsed->path.web) {
        strcat(uri, parsed->path.web);
    }

    if (parsed->query) {
        strcat(uri, "?");
        strcat(uri, parsed->query);
    }

    if (parsed->fragment) {
        strcat(uri, "#");
        strcat(uri, parsed->fragment);
    }

    return uri;
}

static char* em_url_parse_vfs(
    em_dispatch_context_t* context, em_url_t* parsed) {
    if (!em_url_compare(
            EM_URL_COMPARE_ORIGIN,
                &context->url, parsed)) {
        return NULL;
    }

    zval* vroot = zend_hash_str_find(
        &context->environ, ZEND_STRL("VIRTUAL_ROOT"));
    zval* droot = zend_hash_str_find(
        &context->environ, ZEND_STRL("DOCUMENT_ROOT"));

    assert(vroot && droot);
    assert(strstr(
        parsed->path.web, Z_STRVAL_P(vroot)));

    char* vfs = calloc(sizeof(char),
        Z_STRLEN_P(droot) +                               // space for droot
        (strlen(parsed->path.web) -                       // space for path.web
            Z_STRLEN_P(vroot)) +                          // not including vroot
        1);

    memcpy(vfs,
        Z_STRVAL_P(droot), Z_STRLEN_P(droot));
    memcpy(&vfs[Z_STRLEN_P(droot)],
        parsed->path.web + Z_STRLEN_P(vroot),
        strlen(parsed->path.web) -
            Z_STRLEN_P(vroot));

    return vfs;
}

#define em_url_parse_env(context, env, field, unknown) \
do {                                                   \
    zval* __env__ = zend_hash_str_find(                \
        &context->environ, ZEND_STRL(env));            \
    if (__env__) {                                     \
        zval value;                                    \
        ZVAL_COPY(&value, __env__);                    \
        convert_to_string(                             \
            &value);                                   \
        parsed->field =                                \
            strdup(                                    \
                Z_STRVAL(value));                      \
        zval_ptr_dtor(&value);                         \
    } else {                                           \
        parsed->field = strdup(unknown);               \
    }                                                  \
} while(0)

static void em_url_parse_complete(em_dispatch_context_t* context, em_url_t* parsed) {
    if (parsed->scheme.raw) {
        if (0 == strcasecmp(
                parsed->scheme.raw, "http")) {
            parsed->scheme.kind =
                EM_URL_HTTP;
        } else if (0 == strcasecmp(
                parsed->scheme.raw, "https")) {
            parsed->scheme.kind =
                EM_URL_HTTPS;
        } else {
            parsed->scheme.kind =
                EM_URL_HTTP;
        }
    } else {
        parsed->scheme.raw =
            strdup("http");
        parsed->scheme.kind =
            EM_URL_HTTP;
    }

    if (!parsed->port.raw) {
        parsed->port.raw =
            strdup("80");
    }
    parsed->port.number =
        atol(parsed->port.raw);

    parsed->uri      = em_url_parse_uri(parsed);
    parsed->path.vfs = em_url_parse_vfs(context, parsed);
}

static char* em_url_parse_dirname(char* path) {
    if (path[strlen(path) - 1] == '/') {
        return path;
    }
    return dirname(path);
}

static void
    em_url_parse_path_rel(
        em_dispatch_context_t* context,
        em_url_t* parsed) {
    if (parsed->path.web[0] != '/') {
        char* cpath = strdup(
            context->url.path.web);
        char* rpath = em_url_parse_dirname(cpath);
        char* path = parsed->path.web;

        parsed->path.web = calloc(sizeof(char),
             strlen(rpath) +
            (strlen(path)  + 1) +
            1
        );
        strcat(parsed->path.web, "/");
        strcat(parsed->path.web, (rpath[0] == '/') ?
            &rpath[1] : &rpath[0]);
        if (rpath[strlen(rpath) - 1] != '/') {
            strcat(parsed->path.web, "/");
        }
        strcat(parsed->path.web, (path[0] == '/') ?
            &path[1] : &path[0]);
        free(path);
        free(cpath);
    } else {
        zval* vroot = zend_hash_str_find(
            &context->environ, ZEND_STRL("VIRTUAL_ROOT"));

        assert(vroot);

        if ((strlen(parsed->path.web) < Z_STRLEN_P(vroot)) ||
            (memcmp(parsed->path.web,
                Z_STRVAL_P(vroot),
                Z_STRLEN_P(vroot)) != SUCCESS)) {
            char* path = parsed->path.web;
            parsed->path.web = calloc(sizeof(char),
                Z_STRLEN_P(vroot) +
                (strlen(path)  + 1)
            );
            strcat(parsed->path.web, Z_STRVAL_P(vroot));
            strcat(parsed->path.web, (path[0] == '/') ?
                &path[1] : &path[0]);
        }
    }
}

static bool em_url_parse_rel(struct _em_dispatch_context_t* context, const char* url, em_url_t* parsed) {
    pcre2_match_data *matches =
        pcre2_match_data_create_from_pattern(
            __em_url_pattern_rel__, NULL);

    if (!matches) {
        return false;
    }

    int rc = pcre2_match(
        __em_url_pattern_rel__,
        (PCRE2_SPTR8)
            url,
        (PCRE2_SIZE)
            strlen(url),
        0,
        0,
        matches, NULL);

    if (rc < 0) {
        pcre2_match_data_free(matches);
        return false;
    }

    parsed->kind        = EM_URL_REL;

    PCRE2_SIZE *ovector =
        pcre2_get_ovector_pointer(matches);

    __PRE2_EXTRACT__(ovector, 2, 1, path.web);
    __PRE2_EXTRACT__(ovector, 2, 2, query);
    __PRE2_EXTRACT__(ovector, 2, 3, fragment);

    pcre2_match_data_free(matches);

    em_url_parse_env(context,
        "SERVER_SCHEME", scheme.raw,
        "http");
    em_url_parse_env(context,
        "SERVER_HOSTNAME", host,
        "localhost");
    em_url_parse_env(context,
        "SERVER_PORT", port.raw,
        "80");

    em_url_parse_path_rel(context, parsed);

    em_url_parse_complete(context, parsed);

#if 0
    em_url_debug(url, parsed);
#endif

    return true;
}

bool em_url_parse(struct _em_dispatch_context_t* context, const char* url, em_url_t* parsed) {
    memset(parsed, 0, sizeof(em_url_t));

    pcre2_match_data *matches =
        pcre2_match_data_create_from_pattern(
            __em_url_pattern_fqu__, NULL);

    if (!matches) {
        return false;
    }

    int rc = pcre2_match(
        __em_url_pattern_fqu__,
        (PCRE2_SPTR8)
            url,
        (PCRE2_SIZE)
            strlen(url),
        0,
        0,
        matches, NULL);

    if (rc < 0) {
        pcre2_match_data_free(matches);
        return em_url_parse_rel(
            context, url, parsed);
    }

    parsed->kind = EM_URL_FQU;

    PCRE2_SIZE *ovector =
        pcre2_get_ovector_pointer(matches);
    __PRE2_EXTRACT__(ovector, 2, 1, scheme.raw);
    __PRE2_EXTRACT__(ovector, 2, 2, host);
    __PRE2_EXTRACT__(ovector, 2, 3, port.raw);
    __PRE2_EXTRACT__(ovector, 2, 4, path.web);
    __PRE2_EXTRACT__(ovector, 2, 5, query);
    __PRE2_EXTRACT__(ovector, 2, 6, fragment);

    pcre2_match_data_free(matches);

    em_url_parse_complete(context, parsed);

#if 0
    em_url_debug(url, parsed);
#endif

    return true;
}

#define __EM_URL_COMPARE_NUMERIC__(field) do { \
    if (lhs->field != rhs->field) {            \
        return false;                          \
    }                                          \
} while(0)

#define __EM_URL_COMPARE_STRING__(field) do { \
    if ((!lhs->field && rhs->field) ||        \
        (!rhs->field && lhs->field)) {        \
        return false;                         \
    }                                         \
                                              \
    if (lhs->field && rhs->field) {           \
        if (strcasecmp(                       \
                lhs->field,                   \
                rhs->field) != 0) {           \
            return false;                     \
        }                                     \
    }                                         \
} while(0)

bool em_url_compare(
    em_url_compare_t comparator,
    em_url_t* lhs, em_url_t* rhs) {

    if (comparator & EM_URL_COMPARE_SCHEME) {
        __EM_URL_COMPARE_NUMERIC__(scheme.kind);
    }

    if (comparator & EM_URL_COMPARE_HOST) {
        __EM_URL_COMPARE_STRING__(host);
    }

    if (comparator & EM_URL_COMPARE_PORT) {
        __EM_URL_COMPARE_NUMERIC__(port.number);
    }

    if (comparator & EM_URL_COMPARE_PATH) {
        __EM_URL_COMPARE_STRING__(path.web);
    }

    if (comparator & EM_URL_COMPARE_QUERY) {
        __EM_URL_COMPARE_STRING__(query);
    }

    if (comparator & EM_URL_COMPARE_FRAGMENT) {
        __EM_URL_COMPARE_STRING__(fragment);
    }

    if (comparator & EM_URL_COMPARE_URI) {
        __EM_URL_COMPARE_STRING__(uri);
    }

    return true;
}

#undef __EM_URL_COMPARE_NUMERIC__
#undef __EM_URL_COMPARE_STRING__

static char* em_url_string_port(em_url_t* parsed) {
    char* result = calloc(sizeof(char), 
        strlen(parsed->scheme.raw)     + 3       +
        strlen(parsed->host)           +
        strlen(parsed->port.raw)       + 1       +
        strlen(parsed->path.web)       +
        (parsed->query ?
            (strlen(parsed->query)     + 1) : 0) +
        (parsed->fragment ?
            (strlen(parsed->fragment)  + 1) : 0) +
        1);
    strcat(result, parsed->scheme.raw);
    strcat(result, "://");
    strcat(result, parsed->host);
    strcat(result, ":");
    strcat(result, parsed->port.raw);
    strcat(result, parsed->path.web);
    if (parsed->query) {
        strcat(result, "?");
        strcat(result, parsed->query);
    }
    if (parsed->fragment) {
        strcat(result, "#");
        strcat(result, parsed->fragment);
    }
    return result;
}

static char* em_url_string_noport(em_url_t* parsed) {
    char* result = calloc(sizeof(char), 
        strlen(parsed->scheme.raw) + 3 +
        strlen(parsed->host)           +
        strlen(parsed->path.web)       +
        (parsed->query ?
            (strlen(parsed->query)     + 1) : 0) +
        (parsed->fragment ?
            (strlen(parsed->fragment)  + 1) : 0) +
        1);
    strcat(result, parsed->scheme.raw);
    strcat(result, "://");
    strcat(result, parsed->host);
    strcat(result, parsed->path.web);
    if (parsed->query) {
        strcat(result, "?");
        strcat(result, parsed->query);
    }
    if (parsed->fragment) {
        strcat(result, "#");
        strcat(result, parsed->fragment);
    }
    return result;
}

static char* em_url_string_rel(em_url_t* parsed) {
    char* result = calloc(sizeof(char), 
        strlen(parsed->path.web)       +
        (parsed->query ?
            (strlen(parsed->query)     + 1) : 0) +
        (parsed->fragment ?
            (strlen(parsed->fragment)  + 1) : 0) +
        1);
    strcat(result, parsed->path.web);
    if (parsed->query) {
        strcat(result, "?");
        strcat(result, parsed->query);
    }
    if (parsed->fragment) {
        strcat(result, "#");
        strcat(result, parsed->fragment);
    }
    return result;
}

char* em_url_string(em_url_kind_t kind, em_url_t* parsed) {
    if (kind == EM_URL_FQU) {
        if ((parsed->scheme.kind == EM_URL_HTTP  && parsed->port.number == 80) ||
            (parsed->scheme.kind == EM_URL_HTTPS && parsed->port.number == 443)) {
            return em_url_string_noport(parsed);
        } else {
            return em_url_string_port(parsed);
        }
    } else {
        return em_url_string_rel(parsed);
    }
}

void em_url_free(em_url_t* url) {
    if (url->fragment) {
        free(url->fragment);
    }

    if (url->query) {
        free(url->query);
    }

    if (url->path.web) {
        free(url->path.web);
    }

    if (url->path.vfs) {
        free(url->path.vfs);
    }

    if (url->uri) {
        free(url->uri);
    }

    if (url->port.raw) {
        free(url->port.raw);
    }

    if (url->host) {
        free(url->host);
    }

    if (url->scheme.raw) {
        free(url->scheme.raw);
    }

    memset(url, 0, sizeof(em_url_t));
}

void em_url_startup(void) {
    const char *fqu =
        "^(?:(http|https|file)://)" // scheme
        "([^/?#:]+)?"               // host (optional)
        "(?::([0-9]+))?"            // port (optional)
        "(/[^?#]*)?"                // path (optional, starts with /)
        "(?:\\?([^#]*))?"           // query (optional, after ?)
        "(?:#(.*))?$";              // fragment (optional, after #)

    int code;
    PCRE2_SIZE offset;

    __em_url_pattern_fqu__ = pcre2_compile(
        (PCRE2_SPTR)fqu,
                    strlen(fqu),
                    PCRE2_CASELESS | PCRE2_DOTALL,
                    &code,
                    &offset,
                    NULL);

    if (!__em_url_pattern_fqu__) {
        fprintf(stderr,
            "[url] failed to compile "
            "__em_url_pattern_fqu__ %d at %zu\n",
            code, offset);
    }

    const char *rel =
        "^([^?#]+)"       // path (any non-empty string)
        "(?:\\?([^#]*))?" // query (optional)
        "(?:#(.*))?$";    // fragment (optional)

    __em_url_pattern_rel__ = pcre2_compile(
        (PCRE2_SPTR)rel,
                    strlen(rel),
                    PCRE2_CASELESS | PCRE2_DOTALL,
                    &code,
                    &offset,
                    NULL);

    if (!__em_url_pattern_rel__) {
        fprintf(stderr,
            "[url] failed to compile "
            "__em_url_pattern_rel__ %d at %zu\n",
            code, offset);
    }
}

void em_url_shutdown(void) {
    if (__em_url_pattern_fqu__) {
        pcre2_code_free(
            __em_url_pattern_fqu__);
    }

    if (__em_url_pattern_rel__) {
        pcre2_code_free(
            __em_url_pattern_rel__);
    }
}