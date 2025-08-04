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
# Magical required things
########################################################################
.SECONDEXPANSION:
########################################################################
# Public, need to set dir, may want to set configure ...
########################################################################
export EM_PHP_DIR        ?= 
export EM_PHP_CONFIGURE  ?=
export EM_PHP_NPROC      ?= $(nproc)
export EM_PHP_CFLAGS     ?=
export EM_PHP_CFLAGS     += -I$(EM_PHP_DIR)/main
export EM_PHP_CFLAGS     += -I$(EM_PHP_DIR)/Zend
export EM_PHP_CFLAGS     += -I$(EM_PHP_DIR)/TSRM
export EM_PHP_CFLAGS     += -I$(EM_PHP_DIR)/ext/standard
export EM_PHP_CFLAGS     += -I$(EM_PHP_DIR)
########################################################################
# Public, but unlikely to want to set these
########################################################################
export EM_EMSDK_CFLAGS   ?=
export EM_EMSDK_LDFLAGS  ?=
########################################################################
# Private, no possible to set these ...
########################################################################
export EM_ROOT_DIR = $(realpath \
	$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
export EM_SRC_DIR  = $(EM_ROOT_DIR)/src
export EM_MK_DIR   = $(EM_ROOT_DIR)/mk
########################################################################
# Private, toolchain, not necessary to set any of this
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
export EMCMAKE     ?= emcmake
########################################################################
ifeq ($(EM_PHP_DIR),)
    $(error EM_PHP_DIR is not set. Please set it with: \
		make -f em.mk EM_PHP_DIR=/path/to/php-src)
endif
########################################################################
# Load exporting macros
########################################################################
include $(EM_MK_DIR)/export.mk
########################################################################
# Load versioning macros
########################################################################
include $(EM_MK_DIR)/version.mk
########################################################################
# Load recipe macros
########################################################################
include $(EM_MK_DIR)/recipe.mk
########################################################################
# Load baking macros
########################################################################
include $(EM_MK_DIR)/bake.mk
########################################################################
# Find libtool
########################################################################
LIBTOOL     ?= $(realpath $(EM_PHP_DIR)/libtool)
########################################################################
# Load bakers
########################################################################
ifneq ($(filter bake-%, $(MAKECMDGOALS)),)
EM_BAKERS      := $(sort \
	$(patsubst bake-%,%,$(filter bake-%, $(MAKECMDGOALS))))
EM_BAKER_PATHS := $(foreach EM_BAKER,\
	$(EM_BAKERS),$(EM_BAKE_IN)/$(EM_BAKER).mk)
$(foreach EM_BAKER_SEARCH,$(EM_BAKER_PATHS), \
  $(if $(wildcard $(EM_BAKER_SEARCH)), \
	,$(error Baking: $(EM_BAKER_SEARCH) not found in $(EM_BAKE_IN)) \
  ) \
)
include $(EM_BAKER_PATHS)
endif
########################################################################
# Load recipes
########################################################################
ifneq ($(or $(filter with-%, $(MAKECMDGOALS)),$(EM_BAKE_RECIPES)),)
EM_RECIPES      := $(sort \
	$(patsubst with-%,%,$(filter with-%, $(MAKECMDGOALS))) $(EM_BAKE_RECIPES))
EM_RECIPE_PATHS := $(foreach EM_RECIPE,\
	$(EM_RECIPES),$(EM_RECIPE_IN)/$(EM_RECIPE).mk)
$(foreach EM_RECIPE_SEARCH,$(EM_RECIPE_PATHS), \
  $(if $(wildcard $(EM_RECIPE_SEARCH)), \
	,$(error Recipe: $(EM_RECIPE_SEARCH) not found in $(EM_RECIPE_IN)) \
  ) \
)
include $(EM_RECIPE_PATHS)
endif
########################################################################
.PHONY: all debug 
.PHONY: clean clean-objects clean-bin clean-recipes clean-php clean-deps
.PHONY: strip install 
.PHONY: bake-% without-% with-%
########################################################################
ifneq ($(filter debug% clean%, $(MAKECMDGOALS)),)
bake-%:
	@true
without-%:
	@true
with-%:
	@true
else
bake-%: all
	@true
without-%: all
	@true
with-%: all
	@true
endif
########################################################################
all: bin

$(EM_PHP_DIR)/config.deps:
	@$(eval EM_RECIPE_DEP_MISSING :=)
	@$(foreach i,$(shell seq 1 $(words $(EM_RECIPE_DEPS))), \
		$(eval EM_DEP := $(word $(i),$(EM_RECIPE_DEPS))) \
		$(eval EM_REQ := $(word $(i),$(EM_RECIPE_REQS))) \
		$(if $(value $(EM_DEP)), \
			$(info Deps: $(EM_REQ) satisfied by $(EM_DEP) = $($(EM_DEP))), \
			$(eval EM_RECIPE_DEP_MISSING += \
				$(EM_REQ) requires $(EM_DEP)) \
		) \
	)
	@if [ -n "$(strip $(EM_RECIPE_DEP_MISSING))" ]; then \
		echo "$(EM_RECIPE_DEP_MISSING)" | \
			sed 's/^/Deps: Missing required symbol: /'; \
		exit 1; \
	fi
	@if [ ! -f $@ ]; then touch $@; fi

$(EM_PHP_DIR)/config.status: $(EM_PHP_DIR)/config.deps $(EM_RECIPE_TARGETS)
	$(EM_PHP_DIR)/buildconf --force
	@cd $(EM_PHP_DIR) && \
		$(EMCONFIGURE) ./configure $(EM_PHP_CONFIGURE) \
			--disable-all \
			--disable-cgi \
			--disable-cli \
			--disable-phpdbg \
			--enable-embed=static \
			--disable-fiber-asm \
			--without-pcre-jit \
			$(EM_RECIPE_CONFIGURE)

$(EM_SRC_DIR)/stub.lo: $(EM_SRC_DIR)/stub.c $(EM_PHP_DIR)/config.status
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EM_PHP_CFLAGS) $(EM_RECIPE_CFLAGS) $(EM_EMSDK_CFLAGS) \
			-c $(EM_SRC_DIR)/stub.c -o $(EM_SRC_DIR)/stub.lo

$(EM_RECIPE_BUILD_OBJECTS): %.lo: %.c $(EM_PHP_DIR)/config.status
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EM_PHP_CFLAGS) $(EM_RECIPE_CFLAGS) $(EM_EMSDK_CFLAGS) \
			-c $< -o $@

$(EM_RECIPE_LINK_OBJECTS): %.lo: %.c $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EM_PHP_CFLAGS) $(EM_RECIPE_CFLAGS) $(EM_EMSDK_CFLAGS) \
			-c $< -o $@

$(EM_PHP_DIR)/.libs/libphp.a.stamp: $(EM_SRC_DIR)/stub.lo $(EM_RECIPE_BUILD_RULES) $(EM_RECIPE_BUILD_OBJECTS)
	@$(EMMAKE) make -C $(EM_PHP_DIR) -j$(EM_PHP_PROC)
	@touch $@

$(EM_PHP_DIR)/.libs/libphp.a: $(EM_PHP_DIR)/.libs/libphp.a.stamp
	@true

$(EM_SRC_DIR)/request.lo: $(EM_SRC_DIR)/request.c $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EM_PHP_CFLAGS) $(EM_RECIPE_CFLAGS) $(EM_EMSDK_CFLAGS) \
			-c $(EM_SRC_DIR)/request.c -o $(EM_SRC_DIR)/request.lo

$(EM_SRC_DIR)/http.lo: $(EM_SRC_DIR)/http.c $(EM_SRC_DIR)/request.lo $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EM_PHP_CFLAGS) $(EM_RECIPE_CFLAGS) $(EM_EMSDK_CFLAGS) \
			-c $(EM_SRC_DIR)/http.c -o $(EM_SRC_DIR)/http.lo

$(EM_SRC_DIR)/node.lo: $(EM_SRC_DIR)/node.c $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EM_PHP_CFLAGS) $(EM_RECIPE_CFLAGS) $(EM_EMSDK_CFLAGS) \
			-c $(EM_SRC_DIR)/node.c -o $(EM_SRC_DIR)/node.lo

$(EM_SRC_DIR)/path.lo: $(EM_SRC_DIR)/path.c $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EM_PHP_CFLAGS) $(EM_RECIPE_CFLAGS) $(EM_EMSDK_CFLAGS) \
			-c $(EM_SRC_DIR)/path.c -o $(EM_SRC_DIR)/path.lo

$(EM_SRC_DIR)/dir.lo: $(EM_SRC_DIR)/dir.c $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EM_PHP_CFLAGS) $(EM_RECIPE_CFLAGS) $(EM_EMSDK_CFLAGS) \
			-c $(EM_SRC_DIR)/dir.c -o $(EM_SRC_DIR)/dir.lo

$(EM_SRC_DIR)/vfs.lo: $(EM_SRC_DIR)/vfs.c $(EM_SRC_DIR)/dir.lo $(EM_SRC_DIR)/node.lo $(EM_SRC_DIR)/path.lo $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EM_PHP_CFLAGS) $(EM_RECIPE_CFLAGS) $(EM_EMSDK_CFLAGS) \
			-c $(EM_SRC_DIR)/vfs.c -o $(EM_SRC_DIR)/vfs.lo

$(EM_SRC_DIR)/iterator.lo: $(EM_SRC_DIR)/iterator.c $(EM_SRC_DIR)/vfs.lo
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EM_PHP_CFLAGS) $(EM_RECIPE_CFLAGS) $(EM_EMSDK_CFLAGS) \
			-c $(EM_SRC_DIR)/iterator.c -o $(EM_SRC_DIR)/iterator.lo

$(EM_SRC_DIR)/buffer.lo: $(EM_SRC_DIR)/buffer.c $(EM_SRC_DIR)/vfs.lo
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EM_PHP_CFLAGS) $(EM_RECIPE_CFLAGS) $(EM_EMSDK_CFLAGS) \
			-c $(EM_SRC_DIR)/buffer.c -o $(EM_SRC_DIR)/buffer.lo

$(EM_SRC_DIR)/dispatch.lo: $(EM_SRC_DIR)/dispatch.c $(EM_SRC_DIR)/buffer.lo
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EM_PHP_CFLAGS) $(EM_RECIPE_CFLAGS) $(EM_EMSDK_CFLAGS) \
			-c $(EM_SRC_DIR)/dispatch.c -o $(EM_SRC_DIR)/dispatch.lo

$(EM_SRC_DIR)/api.lo: $(EM_SRC_DIR)/api.c $(EM_SRC_DIR)/dispatch.lo $(EM_SRC_DIR)/http.lo $(EM_SRC_DIR)/vfs.lo $(EM_SRC_DIR)/iterator.lo $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --mode=compile --tag=CC \
		$(CC) $(EM_PHP_CFLAGS) $(EM_RECIPE_CFLAGS) $(EM_EMSDK_CFLAGS) \
			-c $(EM_SRC_DIR)/api.c -o $(EM_SRC_DIR)/api.lo

build: $(EM_PHP_DIR)/.libs/libphp.a

api: $(EM_SRC_DIR)/api.lo

bin: $(EM_SRC_DIR)/api.lo $(EM_RECIPE_LINK_RULES) $(EM_RECIPE_LINK_OBJECTS) $(EM_PHP_DIR)/.libs/libphp.a
	$(LIBTOOL) --silent --preserve-dup-deps --mode=link --tag=CC \
	$(CC) -o $(EM_ROOT_DIR)/php-em.js --post-js=$(EM_SRC_DIR)/stub.js $(EM_RECIPE_LINK_OBJECTS) \
		-s EXPORTED_FUNCTIONS='$(EM_EXPORT_FUNCTIONS)' \
		-s EXPORTED_RUNTIME_METHODS='$(EM_EXPORT_METHODS)' \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s INITIAL_MEMORY=128MB \
		-s EXIT_RUNTIME=1 \
		-s WASM=1 $(EM_EMSDK_LDFLAGS) $(EM_RECIPE_LDFLAGS) $(EM_RECIPE_LIBS) \
		$(EM_PHP_DIR)/.libs/libphp.a \
		$(EM_SRC_DIR)/stub.o \
		$(EM_SRC_DIR)/request.o \
		$(EM_SRC_DIR)/http.o \
		$(EM_SRC_DIR)/dir.o \
		$(EM_SRC_DIR)/node.o \
		$(EM_SRC_DIR)/path.o \
		$(EM_SRC_DIR)/vfs.o \
		$(EM_SRC_DIR)/iterator.o \
		$(EM_SRC_DIR)/buffer.o \
		$(EM_SRC_DIR)/dispatch.o \
		$(EM_SRC_DIR)/api.o
	@ls -lash $(EM_ROOT_DIR)/php-em.js $(EM_ROOT_DIR)/php-em.wasm

strip: bin
	$(STRIP) $(EM_ROOT_DIR)/php-em.wasm

install: bin
	@$(EMMAKE) make -C $(EM_PHP_DIR) install

clean-recipes: $(EM_RECIPE_CLEANERS)

clean-objects:
	@rm -rf $(EM_SRC_DIR)/*.o
	@rm -rf $(EM_SRC_DIR)/*.lo
	@rm -rf $(EM_RECIPE_STUBS)/*.o
	@rm -rf $(EM_RECIPE_STUBS)/*.lo

clean-bin:
	@rm -rf $(EM_ROOT_DIR)/php-em.js
	@rm -rf $(EM_ROOT_DIR)/php-em.wasm

clean-php:
	@rm -rf $(EM_PHP_DIR)/config.status
	@rm -rf $(EM_PHP_DIR)/Makefile

clean-deps:
	@rm -rf config.deps

clean: clean-recipes clean-objects clean-bin clean-deps
	@$(EMMAKE) make \
		-C $(EM_PHP_DIR) distclean
	@echo "The build area is clean"

debug:
	@echo "EM_ROOT_DIR:        $(EM_ROOT_DIR)"
	@echo "EM_SRC_DIR:         $(EM_SRC_DIR)"
	@echo "EM_PHP_DIR:         $(EM_PHP_DIR)"
	@echo "EM_PHP_VERSION:     $(EM_PHP_VERSION)"
ifneq ($(EM_PHP_CONFIGURE),)
	@echo "EM_PHP_CONFIGURE:"
	@echo "\t$(EM_PHP_CONFIGURE)"
endif
	@echo "EM_BAKE_IN: $(EM_BAKE_IN)"
ifneq ($(EM_BAKERS),)
	@echo "EM_BAKERS"
	@echo "\t$(EM_BAKERS)"
endif
ifneq ($(EM_BAKE_RECIPES),)
	@echo "EM_BAKE_RECIPES"
	@echo "\t$(EM_BAKE_RECIPES)"
endif
	@echo "EM_RECIPE_IN: $(EM_RECIPE_IN)"
ifneq ($(EM_RECIPES),)
	@echo "EM_RECIPES"
	@echo "\t$(EM_RECIPES)"
endif
ifneq ($(EM_RECIPE_CONFIGURE),)
	@echo "EM_RECIPE_CONFIGURE:"
	@echo "\t$(EM_RECIPE_CONFIGURE)"
endif
ifneq ($(EM_RECIPE_TARGETS),)
	@echo "EM_RECIPE_TARGETS:"
	@echo "\t$(EM_RECIPE_TARGETS)"
endif
ifneq ($(EM_RECIPE_LIBS),)
	@echo "EM_RECIPE_LIBS:"
	@echo "\t$(EM_RECIPE_LIBS)"
endif
ifneq ($(EM_RECIPE_BUILD_OBJECTS),)
	@echo "EM_RECIPE_BUILD_SOURCE:"
	@echo "\t$(EM_RECIPE_BUILD_SOURCE)"
	@echo "EM_RECIPE_BUILD_OBJECTS:"
	@echo "\t$(EM_RECIPE_BUILD_OBJECTS)"
endif
ifneq ($(EM_RECIPE_LINK_OBJECTS),)
	@echo "EM_RECIPE_LINK_SOURCE:"
	@echo "\t$(EM_RECIPE_LINK_SOURCE)"
	@echo "EM_RECIPE_LINK_OBJECTS:"
	@echo "\t$(EM_RECIPE_LINK_OBJECTS)"
endif
ifneq ($(EM_RECIPE_CFLAGS),)
	@echo "EM_RECIPE_CFLAGS:"
	@echo "\t$(EM_RECIPE_CFLAGS)"
endif
ifneq ($(EM_RECIPE_LDFLAGS),)
	@echo "EM_RECIPE_LDFLAGS:"
	@echo "\t$(EM_RECIPE_LDFLAGS)"
endif
ifneq ($(EM_EMSDK_CFLAGS),)
	@echo "EM_EMSDK_CFLAGS:"
	@echo "\t$(EM_EMSDK_CFLAGS)"
endif
ifneq ($(EM_EMSDK_LDFLAGS),)
	@echo "EM_EMSDK_LDFLAGS:"
	@echo "\t$(EM_EMSDK_LDFLAGS)"
endif
########################################################################
# Disable all implicit rules - we handle everything explicitly
########################################################################
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:
# Clear ALL pattern rules and disable all implicit rules
%:: %,v
%:: RCS/%,v
%:: s.%
%:: SCCS/s.%
%.c:: %.w %.ch
%.c:: %.y
%.c:: %.l
%.c:: %.w
%.c:: %.ch
%.o:: %.c
%.o:: %.cc
%.o:: %.cpp
%.o:: %.p
%.o:: %.f
%.lo:: %.c