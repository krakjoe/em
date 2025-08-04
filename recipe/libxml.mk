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
EM_LIBXML_URL   = https://download.gnome.org/sources/libxml2/2.9/libxml2-2.9.9.tar.xz
EM_LIBXML_TAR   = $(EM_RECIPE_OUT)/libxml2-2.9.9.tar.xz
EM_LIBXML_SRC   = $(EM_RECIPE_OUT)/libxml
EM_LIBXML_LIB   = $(EM_LIBXML_SRC)/lib/libxml2.a
EM_LIBXML_INC   = $(EM_LIBXML_SRC)/include
########################################################################
EM_LIBXML_OPTS      ?=
EM_LIBXML_CFLAGS    ?=
EM_LIBXML_LDFLAGS   ?=
EM_LIBXML_CONFIGURE ?= --with-libxml=$(EM_LIBXML_SRC)
########################################################################
EM_LIBXML_CONFIGURED  = $(EM_LIBXML_SRC)/Makefile
EM_LIBXML_MADE        = $(EM_LIBXML_SRC)/libxml2.a
########################################################################
.PHONY: all-libxml clean-libxml
########################################################################
all-libxml: install-libxml

$(EM_LIBXML_TAR): $$(EM_ZLIB_LIB)
	wget $(EM_LIBXML_URL) -O $(EM_LIBXML_TAR)
	mkdir -p $(EM_LIBXML_SRC)
	tar -C $(EM_LIBXML_SRC) --strip-components=1 -xvf \
		$(EM_LIBXML_TAR)
	@touch $@

$(EM_LIBXML_CONFIGURED): $(EM_LIBXML_TAR)
	cd $(EM_LIBXML_SRC) && \
	CFLAGS=$(EM_LIBXML_CFLAGS) LDFLAGS=$(EM_LIBXML_LDFLAGS) \
		$(EMCONFIGURE) ./configure \
			--disable-shared \
			--enable-static \
			--with-reader \
			--with-writer \
			--without-iconv \
			--without-icu \
			--with-zlib=$(EM_ZLIB_SRC) \
			--prefix=$(EM_LIBXML_SRC) \
			$(EM_LIBXML_OPTS)
	@touch $@

$(EM_LIBXML_MADE): $(EM_LIBXML_CONFIGURED)
	cd $(EM_LIBXML_SRC) && \
	CFLAGS=$(EM_LIBXML_CFLAGS) LDFLAGS=$(EM_LIBXML_LDFLAGS) \
		$(EMMAKE) make -j$(EM_PHP_PROC)
	@touch $@

$(EM_LIBXML_LIB): $(EM_LIBXML_MADE)
	cd $(EM_LIBXML_SRC) && \
	CFLAGS=$(EM_LIBXML_CFLAGS) LDFLAGS=$(EM_LIBXML_LDFLAGS) \
		$(EMMAKE) make install

clean-libxml:
	@rm -rf $(EM_LIBXML_TAR)
	@rm -rf $(EM_LIBXML_SRC)

########################################################################
# Setup recipe
########################################################################
$(eval $(call EM_RECIPE_ADD_CONFIGURE, $(EM_LIBXML_CONFIGURE)))
$(eval $(call EM_RECIPE_ADD_TARGET,    $(EM_LIBXML_LIB)))
$(eval $(call EM_RECIPE_ADD_LIB,       $(EM_LIBXML_LIB)))
$(eval $(call EM_RECIPE_ADD_CLEANER,   clean-libxml))
$(eval $(call EM_RECIPE_ADD_DEP,       libxml, EM_ZLIB_SRC))
$(eval $(call EM_RECIPE_ADD_DEP,       libxml, EM_ZLIB_LIB))
########################################################################
# Export for php
########################################################################
export LIBXML_LIBS       = $(EM_LIBXML_LIB)
export LIBXML_CFLAGS     = -I$(EM_LIBXML_INC)

export Z_LIBS            = $(EM_ZLIB_LIB)
export Z_CFLAGS          = -I$(EM_ZLIB_SRC)