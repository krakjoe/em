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
# Public, need to set this ...
########################################################################
export EM_PHP_DIR  ?= 
########################################################################
# Private, but probably not necessary to set in most cases
########################################################################
export EM_ROOT_DIR = $(realpath \
	$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
export EM_SRC_DIR  = $(EM_ROOT_DIR)/src
########################################################################
# Private, toolchain, unlikely to be necessary to set any of this
########################################################################
export CC=emcc
export CXX=em++
export AR=emar
export RANLIB=emranlib
export STRIP=emstrip
export CFLAGS=-DEMSCRIPTEN -DHAVE_REALLOCARRAY -O2
export EXTRA_LIBS=$(EM_SRC_DIR)/stub.lo
########################################################################
# Super duper private, probably stuff will break if caller sets these
########################################################################
export EMCONFIGURE ?= emconfigure
export EMMAKE      ?= emmake
export EMCFLAGS    ?=
export EMCFLAGS    += -I$(EM_PHP_DIR)/main
export EMCFLAGS    += -I$(EM_PHP_DIR)/Zend
export EMCFLAGS    += -I$(EM_PHP_DIR)/TSRM
export EMCFLAGS    += -I$(EM_PHP_DIR)/ext/standard
export EMCFLAGS    += -I$(EM_PHP_DIR)
########################################################################
# Protected, should probably not be set by caller, may be set by recipes
########################################################################
EM_EXTRA_TMP       ?= /tmp
EM_EXTRA_NPROC     ?= $(nproc)
EM_EXTRA_CONFIGURE ?=
EM_EXTRA_COMPILE   ?=
EM_EXTRA_LINK      ?=
########################################################################
# Public, may be set by caller
########################################################################
EM_EXTRA_RECIPE_IN      ?= $(EM_ROOT_DIR)/recipe
EM_EXTRA_RECIPE_STUBS   ?= $(EM_EXTRA_RECIPE_IN)/stubs
EM_EXTRA_RECIPE_OUT     ?= /tmp
########################################################################
# Private, used in recipes
########################################################################
EM_EXTRA_RECIPE_TARGETS  ?=
EM_EXTRA_RECIPE_CLEANERS ?=
########################################################################
EM_RECIPE_BUILD_STUBS        ?=
EM_RECIPE_BUILD_STUB_OBJECTS ?=
EM_RECIPE_LINK_STUBS         ?=
EM_RECIPE_LINK_STUB_OBJECTS  ?=
########################################################################
define EM_RECIPE_ADD_BUILD_STUB
ifeq ($(filter $(1),$(EM_RECIPE_BUILD_STUBS)),)
EM_RECIPE_BUILD_STUBS += $(1)
EM_RECIPE_BUILD_STUB_OBJECTS += $(1:.c=.lo)
endif
endef
########################################################################
define EM_RECIPE_ADD_LINK_STUB
ifeq ($(filter $(1),$(EM_RECIPE_LINK_STUBS)),)
EM_RECIPE_LINK_STUBS += $(1)
EM_RECIPE_LINK_STUB_OBJECTS += $(1:.c=.lo)
endif
endef
########################################################################
ifeq ($(EM_PHP_DIR),)
    $(error EM_PHP_DIR is not set. Please set it with: \
		make -f em.mk EM_PHP_DIR=/path/to/php-src)
endif
EM_PHP_VERSION := $(shell \
	awk '/PHP_VERSION_ID/ {print $$3}' \
		$(EM_PHP_DIR)/main/php_version.h)
define EM_PHP_VERSION_GE
$(shell test $(EM_PHP_VERSION) -ge $(1) && echo true || echo false)
endef

define EM_PHP_VERSION_GT
$(shell test $(EM_PHP_VERSION) -gt $(1) && echo true || echo false)
endef

define EM_PHP_VERSION_LT
$(shell test $(EM_PHP_VERSION) -lt $(1) && echo true || echo false)
endef

define EM_PHP_VERSION_LE
$(shell test $(EM_PHP_VERSION) -le $(1) && echo true || echo false)
endef
########################################################################
# Find libtool
########################################################################
LIBTOOL     ?= $(realpath $(EM_PHP_DIR)/libtool)
########################################################################
# Load recipes
########################################################################
ifneq ($(filter with-%, $(MAKECMDGOALS)),)
EM_RECIPES      := $(sort \
	$(patsubst with-%,%,$(filter with-%, $(MAKECMDGOALS))))
EM_RECIPE_PATHS := $(foreach r,\
	$(EM_RECIPES),$(EM_EXTRA_RECIPE_IN)/$(r).mk)
$(foreach p,$(EM_RECIPE_PATHS), \
  $(if $(wildcard $(p)), \
	,$(error Recipe: $(p) not found in recipe/) \
  ) \
)
include $(EM_RECIPE_PATHS)
endif
########################################################################
.PHONY: all debug clean clean-objects clean-bin clean-recipes clean-php strip install with-%
########################################################################
ifneq ($(filter debug% clean%, $(MAKECMDGOALS)),)
with-%:
	@true
else
with-%: all
	@true
endif
########################################################################
all: bin

$(EM_PHP_DIR)/config.status: $(EM_EXTRA_RECIPE_TARGETS)
	$(EM_PHP_DIR)/buildconf --force
	@cd $(EM_PHP_DIR) && \
		$(EMCONFIGURE) ./configure \
			--disable-all \
			--disable-cgi \
			--disable-cli \
			--disable-phpdbg \
			--enable-embed=static \
			--disable-fiber-asm \
			--without-pcre-jit \
			$(EM_EXTRA_CONFIGURE)

$(EM_SRC_DIR)/stub.lo: $(EM_SRC_DIR)/stub.c $(EM_PHP_DIR)/config.status
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EMCFLAGS) $(EM_EXTRA_COMPILE) \
			-c $(EM_SRC_DIR)/stub.c -o $(EM_SRC_DIR)/stub.lo

$(EM_RECIPE_BUILD_STUB_OBJECTS): %.lo: %.c $(EM_PHP_DIR)/config.status
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EMCFLAGS) $(EM_EXTRA_COMPILE) \
			-c $< -o $@

$(EM_RECIPE_LINK_STUB_OBJECTS): %.lo: %.c $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EMCFLAGS) $(EM_EXTRA_COMPILE) \
			-c $< -o $@

$(EM_PHP_DIR)/.libs/libphp.a.stamp: $(EM_SRC_DIR)/stub.lo $(EM_RECIPE_BUILD_STUB_OBJECTS)
	@$(EMMAKE) make -C $(EM_PHP_DIR) -j$(EM_EXTRA_NPROC)
	@touch $@

$(EM_PHP_DIR)/.libs/libphp.a: $(EM_PHP_DIR)/.libs/libphp.a.stamp
	@true

$(EM_SRC_DIR)/http.lo: $(EM_SRC_DIR)/http.c $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EMCFLAGS) $(EM_EXTRA_COMPILE) \
			-c $(EM_SRC_DIR)/http.c -o $(EM_SRC_DIR)/http.lo

$(EM_SRC_DIR)/node.lo: $(EM_SRC_DIR)/node.c $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EMCFLAGS) $(EM_EXTRA_COMPILE) \
			-c $(EM_SRC_DIR)/node.c -o $(EM_SRC_DIR)/node.lo

$(EM_SRC_DIR)/path.lo: $(EM_SRC_DIR)/path.c $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EMCFLAGS) $(EM_EXTRA_COMPILE) \
			-c $(EM_SRC_DIR)/path.c -o $(EM_SRC_DIR)/path.lo

$(EM_SRC_DIR)/dir.lo: $(EM_SRC_DIR)/dir.c $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EMCFLAGS) $(EM_EXTRA_COMPILE) \
			-c $(EM_SRC_DIR)/dir.c -o $(EM_SRC_DIR)/dir.lo

$(EM_SRC_DIR)/vfs.lo: $(EM_SRC_DIR)/vfs.c $(EM_SRC_DIR)/dir.lo $(EM_SRC_DIR)/node.lo $(EM_SRC_DIR)/path.lo $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EMCFLAGS) $(EM_EXTRA_COMPILE) \
			-c $(EM_SRC_DIR)/vfs.c -o $(EM_SRC_DIR)/vfs.lo

$(EM_SRC_DIR)/api.lo: $(EM_SRC_DIR)/api.c $(EM_SRC_DIR)/http.lo $(EM_SRC_DIR)/vfs.lo $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EMCFLAGS) $(EM_EXTRA_COMPILE) \
			-c $(EM_SRC_DIR)/api.c -o $(EM_SRC_DIR)/api.lo

build: $(EM_PHP_DIR)/.libs/libphp.a

api: $(EM_SRC_DIR)/api.lo

bin: $(EM_SRC_DIR)/api.lo $(EM_RECIPE_LINK_STUB_OBJECTS) $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --preserve-dup-deps --mode=link --tag=CC \
	$(CC) -o $(EM_ROOT_DIR)/php-em.js --post-js=$(EM_SRC_DIR)/stub.js $(EM_RECIPE_LINK_STUB_OBJECTS) \
		-s EXPORTED_FUNCTIONS='["_em_startup", "_em_run_string", "_em_run_length", "_em_run_free", "_em_shutdown", "_em_http_buffer", "_em_vfs_reset"]' \
		-s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","stringToUTF8", "lengthBytesUTF8", "HEAPU8"]' \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s INITIAL_MEMORY=128MB \
		-s WASM=1 $(EM_EXTRA_LINK) \
		$(EM_PHP_DIR)/.libs/libphp.a \
		$(EM_SRC_DIR)/stub.o \
		$(EM_SRC_DIR)/http.o \
		$(EM_SRC_DIR)/dir.o \
		$(EM_SRC_DIR)/node.o \
		$(EM_SRC_DIR)/path.o \
		$(EM_SRC_DIR)/vfs.o \
		$(EM_SRC_DIR)/api.o
	@ls -lash $(EM_ROOT_DIR)/php-em.js $(EM_ROOT_DIR)/php-em.wasm

strip: bin
	$(STRIP) $(EM_ROOT_DIR)/php-em.wasm

install: bin
	@$(EMMAKE) make -C $(EM_PHP_DIR) install

clean-recipes: $(EM_EXTRA_RECIPE_CLEANERS)

clean-objects:
	@rm -rf $(EM_SRC_DIR)/*.o
	@rm -rf $(EM_SRC_DIR)/*.lo

clean-bin:
	@rm -rf $(EM_ROOT_DIR)/php-em.js
	@rm -rf $(EM_ROOT_DIR)/php-em.wasm

clean-php:
	@$(EMMAKE) make -C $(EM_PHP_DIR) clean

clean: clean-recipes clean-objects clean-bin clean-php
	@echo "The build area is clean"

debug:
	@echo "EM_ROOT_DIR:        $(EM_ROOT_DIR)"
	@echo "EM_SRC_DIR:         $(EM_SRC_DIR)"
	@echo "EM_PHP_DIR:         $(EM_PHP_DIR)"
	@echo "EM_PHP_VERSION:     $(EM_PHP_VERSION)"
	@echo "EM_EXTRA_RECIPE_IN: $(EM_EXTRA_RECIPE_IN)"
ifneq ($(EM_EXTRA_RECIPE_TARGETS),)
	@echo "EM_EXTRA_RECIPE_TARGETS:"
	@echo "\t$(EM_EXTRA_RECIPE_TARGETS)"
endif
ifneq ($(EM_EXTRA_CONFIGURE),)
	@echo "EM_EXTRA_CONFIGURE:"
	@echo "\t$(EM_EXTRA_CONFIGURE)"
endif
ifneq ($(EM_EXTRA_COMPILE),)
	@echo "EM_EXTRA_COMPILE:"
	@echo "\t$(EM_EXTRA_COMPILE)"
endif
ifneq ($(EM_EXTRA_LINK),)
	@echo "EM_EXTRA_LINK:"
	@echo "\t$(EM_EXTRA_LINK)"
endif
ifneq ($(EM_RECIPE_BUILD_STUB_OBJECTS),)
	@echo "EM_RECIPE_BUILD_STUB_OBJECTS:"
	@echo "\t$(EM_RECIPE_BUILD_STUB_OBJECTS)"
endif
ifneq ($(EM_RECIPE_LINK_STUB_OBJECTS),)
	@echo "EM_RECIPE_LINK_STUB_OBJECTS:"
	@echo "\t$(EM_RECIPE_LINK_STUB_OBJECTS)"
endif