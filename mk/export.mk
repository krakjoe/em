########################################################################
# em                                                                   #
########################################################################
# Copyright (c) Joe Watkins 2025                                       #
########################################################################
# This source file is subject to version 3.01 of the PHP license,      #
# that is bundled with this package in the file LICENSE, and is        #
# available through the world-wide-web at the following url:           #
# http://www.php.net/license/3_01.txt                                  #
# If you did not receive a copy of the PHP license and are unable to   #
# obtain it through the world-wide-web, please send a note to          #
# license@php.net so we can mail you a copy immediately.               #
########################################################################
# Author: krakjoe                                                      #
########################################################################
# Shall define vars for exports and export macros for use in recipes
########################################################################
#
# Exported variables
#  EM_EXPORT_FUNCTIONS list of functions to export at link time
#  EM_EXPORT_METHODS   list of methods to export at link time
#
# Exported macros
#   EM_EXPORT_ADD_FUNCTION(function)
#     Shall append the function to the export list
#   EM_EXPORT_ADD_METHOD(method)
#     Shall append the method to the export list
#
########################################################################
# EM_EXPORT_AD_FUNCTION
########################################################################
EM_EXPORT_FUNCTIONS = [          \
	"_em_startup",               \
	"_em_run_string",            \
	"_em_run_script",            \
	"_em_run_request",           \
	"_em_run_length",            \
	"_em_run_free",              \
	"_em_shutdown",              \
	"_em_http_request_timeout",  \
	"_em_http_request_error",    \
	"_em_http_request_response", \
	"_em_vfs_put",               \
	"_em_vfs_unlink",            \
	"_em_vfs_mkdir",             \
	"_em_vfs_move",              \
	"_em_vfs_get_address",       \
	"_em_vfs_get_length",        \
	"_em_vfs_reset",             \
	"_em_vfs_iterator",          \
	"_em_vfs_iterator_count",    \
	"_em_vfs_iterator_kind",     \
	"_em_vfs_iterator_name",     \
	"_em_vfs_iterator_length",   \
	"_em_vfs_iterator_address",  \
	"_em_vfs_iterator_created",  \
	"_em_vfs_iterator_modified", \
	"_em_vfs_iterator_reset",    \
	"_em_vfs_iterator_next",     \
	"_em_vfs_iterator_free",     \
	"_em_vfs_memory_alloc",      \
	"_em_vfs_memory_write",      \
	"_em_vfs_memory_free",       \
	"_select",                   \
	"_malloc", "_free"           \
]
define EM_EXPORT_ADD_FUNCTION
$(eval EM_EXPORT := $(strip $(1)))
ifeq ($(filter \
	$(EM_EXPORT),\
		$(EM_EXPORT_FUNCTIONS)),)
EM_EXPORT_FUNCTIONS  += $(EM_EXPORT)
endif
endef
########################################################################
# EM_EXPORT_ADD_METHOD
########################################################################
EM_EXPORT_METHODS = [      \
	"ccall",               \
	"cwrap",               \
	"UTF8ToString",        \
	"stringToUTF8",        \
	"lengthBytesUTF8",     \
	"HEAPU8",              \
	"HEAP32"               \
]
define EM_EXPORT_ADD_METHOD
$(eval EM_EXPORT := $(strip $(1)))
ifeq ($(filter \
	$(EM_EXPORT),\
		$(EM_EXPORT_METHODS)),)
EM_EXPORT_METHODS  += $(EM_EXPORT)
endif
endef
########################################################################
# EM_EXPORT_ADD_BLACKLIST
########################################################################
EM_EXPORT_BLACKLIST = [    \
	"em_buffer_join",      \
	"em_buffer_write",     \
	"em_buffer_clear",     \
	"php_request_startup", \
	"php_request_shutdown" \
]
define EM_EXPORT_ADD_BLACKLIST
$(eval EM_EXPORT := $(strip $(1)))
ifeq ($(filter \
	$(EM_EXPORT),\
		$(EM_EXPORT_BLACKLIST)),)
EM_EXPORT_BLACKLIST  += $(EM_EXPORT)
endif
endef
########################################################################