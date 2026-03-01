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
EM_ZLIB_URL   = https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
EM_ZLIB_TAR   = $(EM_RECIPE_OUT)/zlib-1.3.1.tar.gz
EM_ZLIB_SRC   = $(EM_RECIPE_OUT)/zlib
EM_ZLIB_LIB   = $(EM_ZLIB_SRC)/lib/libz.a
EM_ZLIB_INC   = $(EM_ZLIB_SRC)/include
########################################################################
EM_ZLIB_OPTS      ?=
EM_ZLIB_CFLAGS    ?=
EM_ZLIB_LDFLAGS   ?=
EM_ZLIB_CONFIGURE ?= --with-zlib=static,$(EM_ZLIB_SRC)
########################################################################
EM_ZLIB_CONFIGURED  = $(EM_ZLIB_SRC)/Makefile
EM_ZLIB_MADE        = $(EM_ZLIB_SRC)/libz.a
########################################################################
.PHONY: clean-zlib
########################################################################
$(EM_ZLIB_TAR):
	wget $(EM_ZLIB_URL) -O $(EM_ZLIB_TAR)
	mkdir -p $(EM_ZLIB_SRC)
	tar -C $(EM_ZLIB_SRC) --strip-components=1 -xvzf \
		$(EM_ZLIB_TAR)

$(EM_ZLIB_CONFIGURED): $(EM_ZLIB_TAR)
	cd $(EM_ZLIB_SRC) && \
	CFLAGS=$(EM_ZLIB_CFLAGS) LDFLAGS=$(EM_ZLIB_LDFLAGS) \
		$(EMCONFIGURE) ./configure \
			--static \
			--prefix=$(EM_ZLIB_SRC) \
			$(EM_ZLIB_OPTS)
	@touch $@

$(EM_ZLIB_MADE): $(EM_ZLIB_CONFIGURED)
	cd $(EM_ZLIB_SRC) && \
	CFLAGS=$(EM_ZLIB_CFLAGS) LDFLAGS=$(EM_ZLIB_LDFLAGS) \
		$(EMMAKE) make -j$(EM_PHP_PROC)

$(EM_ZLIB_LIB): $(EM_ZLIB_MADE)
	cd $(EM_ZLIB_SRC) && \
	CFLAGS=$(EM_ZLIB_CFLAGS) LDFLAGS=$(EM_ZLIB_LDFLAGS) \
		$(EMMAKE) make install

clean-zlib:
	@rm -rf $(EM_ZLIB_TAR)
	@rm -rf $(EM_ZLIB_SRC)

########################################################################
# Setup recipe
########################################################################
$(eval $(call EM_RECIPE_ADD_CONFIGURE, $(EM_ZLIB_CONFIGURE)))
$(eval $(call EM_RECIPE_ADD_TARGET,    $(EM_ZLIB_LIB)))
$(eval $(call EM_RECIPE_ADD_LIB,       $(EM_ZLIB_LIB)))
$(eval $(call EM_RECIPE_ADD_CFLAGS,    -DHAVE_EM_ZLIB))
$(eval $(call EM_RECIPE_ADD_CFLAGS,    -I$(EM_ZLIB_SRC)/include))
$(eval $(call EM_RECIPE_ADD_CLEANER,   clean-zlib))
########################################################################
# Export for php
########################################################################
export ZLIB_LIBS       = $(EM_ZLIB_LIB)
export ZLIB_CFLAGS     = -I$(EM_ZLIB_INC)