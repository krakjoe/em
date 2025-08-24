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

#ifndef HAVE_EM_URL
#define HAVE_EM_URL

#include <inttypes.h>
#include <stdbool.h>

typedef enum _em_url_kind_t {
    EM_URL_UNUSED,
    EM_URL_FQU,
    EM_URL_REL
} em_url_kind_t;

typedef enum _em_url_scheme_t {
    EM_URL_FILE,
    EM_URL_HTTP,
    EM_URL_HTTPS
} em_url_scheme_t;

typedef enum _em_url_compare_t {
    EM_URL_COMPARE_SCHEME      = 1,
    EM_URL_COMPARE_HOST        = 2,
    EM_URL_COMPARE_PORT        = 4,
    EM_URL_COMPARE_PATH        = 8,
    EM_URL_COMPARE_QUERY       = 16,
    EM_URL_COMPARE_FRAGMENT    = 32,
    EM_URL_COMPARE_URI         = 64
} em_url_compare_t;

#define EM_URL_COMPARE_ORIGIN \
    EM_URL_COMPARE_SCHEME |   \
    EM_URL_COMPARE_HOST   |   \
    EM_URL_COMPARE_PORT

typedef struct _em_url_t {
    em_url_kind_t kind;
    struct {
        em_url_scheme_t kind;
        char*           raw;
    } scheme;
    char* host;
    struct {
        uint32_t number;
        char*    raw;
    } port;
    struct {
        char*    web;
        char*    vfs;
    } path;
    char* query;
    char* fragment;
    char* uri;
} em_url_t;

typedef struct _em_dispatch_context_t _em_dispatch_context_t;

bool em_url_parse(
    struct _em_dispatch_context_t* context,
    const char* url, em_url_t* parsed);
char* em_url_string(
    em_url_kind_t kind,
    em_url_t* parsed);
bool em_url_compare(
    em_url_compare_t comparator,
    em_url_t* lhs, em_url_t* rhs);
void em_url_free(em_url_t* url);

void em_url_startup(void);
void em_url_shutdown(void);
#endif