########################################################################
# em                                                                  #
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
EM_PNG_URL   = https://download.sourceforge.net/libpng/libpng-1.6.50.tar.gz
EM_PNG_TAR   = $(EM_RECIPE_OUT)/libpng-1.6.50.tar.gz
EM_PNG_SRC   = $(EM_RECIPE_OUT)/libpng
EM_PNG_LIB   = $(EM_PNG_SRC)/lib/libpng.a
EM_PNG_INC   = $(EM_PNG_SRC)/include
########################################################################
EM_PNG_OPTS      ?=
EM_PNG_CFLAGS    ?= -I$(EM_ZLIB_INC)
EM_PNG_LDFLAGS   ?= -L$(EM_ZLIB_SRC)/lib
########################################################################
EM_PNG_CONFIGURED  = $(EM_PNG_SRC)/Makefile
EM_PNG_MADE        = $(EM_PNG_SRC)/libpng.a
########################################################################
.PHONY: clean-png
########################################################################
$(EM_PNG_TAR): $$(EM_ZLIB_LIB)
	wget $(EM_PNG_URL) -O $(EM_PNG_TAR)
	mkdir -p $(EM_PNG_SRC)
	tar -C $(EM_PNG_SRC) --strip-components=1 -xvzf \
		$(EM_PNG_TAR)
	@touch $(EM_PNG_TAR)

$(EM_PNG_CONFIGURED): $(EM_PNG_TAR)
	cd $(EM_PNG_SRC) && \
		CFLAGS="$(EM_PNG_CFLAGS)" LDFLAGS="$(EM_PNG_LDFLAGS)" \
		CPP="$(CC) -E ${EM_PNG_CFLAGS} $(EM_PNG_LDFLAGS)" \
			$(EMCONFIGURE) ./configure \
				--disable-shared \
				--enable-static \
				--disable-tools \
				--disable-tests \
				--enable-hardware-optimizations=no \
				--with-zlib-prefix=$(EM_ZLIB_SRC) \
				--prefix=$(EM_PNG_SRC) \
				$(EM_PNG_OPTS)
	@touch $@

$(EM_PNG_MADE): $(EM_PNG_CONFIGURED)
	cd $(EM_PNG_SRC) && \
		CFLAGS="$(EM_PNG_CFLAGS)" LDFLAGS="$(EM_PNG_LDFLAGS)" \
		CPP="$(CC) -E ${EM_PNG_CFLAGS} $(EM_PNG_LDFLAGS)" \
			$(EMMAKE) make -j$(EM_PHP_PROC)
	@touch $@

$(EM_PNG_LIB): $(EM_PNG_MADE)
	cd $(EM_PNG_SRC) && \
		CFLAGS="$(EM_PNG_CFLAGS)" LDFLAGS="$(EM_PNG_LDFLAGS)" \
		CPP="$(CC) -E ${EM_PNG_CFLAGS} $(EM_PNG_LDFLAGS)" \
			$(EMMAKE) make install
	@touch $@

clean-png:
	@rm -rf $(EM_PNG_TAR)
	@rm -rf $(EM_PNG_SRC)

########################################################################
# Setup recipe
########################################################################
$(eval $(call EM_RECIPE_ADD_BUILD_RULE, $(EM_PNG_LIB)))
$(eval $(call EM_RECIPE_ADD_DEP,        png, $(EM_ZLIB_SRC)))
$(eval $(call EM_RECIPE_ADD_DEP,        png, $(EM_ZLIB_LIB)))
$(eval $(call EM_RECIPE_ADD_DEP,        png, $(EM_ZLIB_INC)))
$(eval $(call EM_RECIPE_ADD_LIB,        $(EM_PNG_LIB)))
$(eval $(call EM_RECIPE_ADD_CLEANER,    clean-png))
########################################################################
# Export for php
########################################################################
export ZLIB_LIBS      = $(EM_ZLIB_LIB)
export ZLIB_CFLAGS    = -I$(EM_ZLIB_INC)
export PNG_LIBS       = $(EM_PNG_LIB)
export PNG_CFLAGS     = -I$(EM_PNG_INC)
########################################################################
# Inject for autoconf
########################################################################
export ac_cv_func_zlibVersion=yes
export ac_cv_lib_z_zlibVersion=yes
export ac_cv_header_zlib_h=yes