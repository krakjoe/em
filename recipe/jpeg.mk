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
EM_JPEG_URL   = https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.1.1/libjpeg-turbo-3.1.1.tar.gz
EM_JPEG_TAR   = $(EM_RECIPE_OUT)/libjpeg-turbo-3.1.1.tar.gz
EM_JPEG_SRC   = $(EM_RECIPE_OUT)/libjpeg
EM_JPEG_LIB   = $(EM_JPEG_SRC)/lib/libjpeg.a
EM_JPEG_INC   = $(EM_JPEG_SRC)/include
########################################################################
EM_JPEG_OPTS      ?=
EM_JPEG_CFLAGS    ?=
EM_JPEG_LDFLAGS   ?=
EM_JPEG_CONFIGURE ?= --with-jpeg=$(EM_JPEG_SRC)
########################################################################
EM_JPEG_CONFIGURED  = $(EM_JPEG_SRC)/Makefile
EM_JPEG_MADE        = $(EM_JPEG_SRC)/libjpeg.a
########################################################################
.PHONY: clean-jpeg
########################################################################
$(EM_JPEG_TAR):
	wget $(EM_JPEG_URL) -O $(EM_JPEG_TAR)
	mkdir -p $(EM_JPEG_SRC)
	tar -C $(EM_JPEG_SRC) --strip-components=1 -xvzf \
		$(EM_JPEG_TAR)
	@touch $(EM_JPEG_TAR)

$(EM_JPEG_CONFIGURED): $(EM_JPEG_TAR)
	mkdir -p $(EM_JPEG_SRC)/build && \
		cd $(EM_JPEG_SRC)/build && \
			$(EMCMAKE) cmake .. \
				-DCMAKE_CROSSCOMPILING_EMULATOR="node" \
				-DCMAKE_INSTALL_PREFIX=$(EM_JPEG_SRC) \
				-DCMAKE_BUILD_TYPE=Release \
				-DBUILD_SHARED_LIBS=OFF \
				-DENABLE_SHARED=OFF \
				-DENABLE_STATIC=ON \
				-DREQUIRE_SIMD=OFF \
				-DWITH_SIMD=OFF \
				-DWITH_TURBOJPEG=OFF
	@touch $@

$(EM_JPEG_MADE): $(EM_JPEG_CONFIGURED)
	cd $(EM_JPEG_SRC)/build && \
		CFLAGS="$(EM_JPEG_CFLAGS)" LDFLAGS="$(EM_JPEG_LDFLAGS)" \
			$(EMMAKE) make -j$(EM_PHP_PROC)
	@touch $@

$(EM_JPEG_LIB): $(EM_JPEG_MADE)
	cd $(EM_JPEG_SRC)/build && \
		CFLAGS="$(EM_JPEG_CFLAGS)" LDFLAGS="$(EM_JPEG_LDFLAGS)" \
			$(EMMAKE) make install
	@touch $@

clean-jpeg:
	@rm -rf $(EM_JPEG_TAR)
	@rm -rf $(EM_JPEG_SRC)

########################################################################
# Setup recipe
########################################################################
$(eval $(call EM_RECIPE_ADD_CONFIGURE,  $(EM_JPEG_CONFIGURE)))
$(eval $(call EM_RECIPE_ADD_TARGET,     $(EM_JPEG_LIB)))
$(eval $(call EM_RECIPE_ADD_LIB,        $(EM_JPEG_LIB)))
$(eval $(call EM_RECIPE_ADD_CLEANER,    clean-jpeg))
########################################################################
# Export for php
########################################################################
export JPEG_LIBS       = $(EM_JPEG_LIB)
export JPEG_CFLAGS     = -I$(EM_JPEG_INC)