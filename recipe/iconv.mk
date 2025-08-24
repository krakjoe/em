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
EM_ICONV_URL   = https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.18.tar.gz
EM_ICONV_TAR   = $(EM_RECIPE_OUT)/libiconv-1.18.tar.gz
EM_ICONV_SRC   = $(EM_RECIPE_OUT)/libiconv
EM_ICONV_LIB   = $(EM_ICONV_SRC)/lib/libiconv.a
EM_ICONV_INC   = $(EM_ICONV_SRC)/include
########################################################################
EM_ICONV_OPTS      ?=
EM_ICONV_CFLAGS    ?=
EM_ICONV_LDFLAGS   ?=
EM_ICONV_CONFIGURE ?= --with-iconv=$(EM_ICONV_SRC)
########################################################################
EM_ICONV_CONFIGURED  = $(EM_ICONV_SRC)/Makefile
EM_ICONV_MADE        = $(EM_ICONV_SRC)/libiconv.a
########################################################################
.PHONY: clean-iconv
########################################################################
$(EM_ICONV_TAR):
	wget $(EM_ICONV_URL) -O $(EM_ICONV_TAR)
	mkdir -p $(EM_ICONV_SRC)
	tar -C $(EM_ICONV_SRC) --strip-components=1 -xvzf \
		$(EM_ICONV_TAR)
	@touch $(EM_ICONV_TAR)

$(EM_ICONV_CONFIGURED): $(EM_ICONV_TAR)
	cd $(EM_ICONV_SRC) && \
		$(EMCONFIGURE) ./configure \
			--prefix=$(EM_ICONV_SRC) \
			--enable-shared=no \
			--enable-static=yes
	@touch $@

$(EM_ICONV_MADE): $(EM_ICONV_CONFIGURED)
	cd $(EM_ICONV_SRC) && \
		CFLAGS="$(EM_ICONV_CFLAGS)" LDFLAGS="$(EM_ICONV_LDFLAGS)" \
			$(EMMAKE) make -j$(EM_PHP_PROC)
	@touch $@

$(EM_ICONV_LIB): $(EM_ICONV_MADE)
	cd $(EM_ICONV_SRC) && \
		CFLAGS="$(EM_ICONV_CFLAGS)" LDFLAGS="$(EM_ICONV_LDFLAGS)" \
			$(EMMAKE) make install
	@touch $@

clean-iconv:
	@rm -rf $(EM_ICONV_TAR)
	@rm -rf $(EM_ICONV_SRC)

########################################################################
# Setup recipe
########################################################################
$(eval $(call EM_RECIPE_ADD_CONFIGURE,  $(EM_ICONV_CONFIGURE)))
$(eval $(call EM_RECIPE_ADD_TARGET,     $(EM_ICONV_LIB)))
$(eval $(call EM_RECIPE_ADD_LIB,        $(EM_ICONV_LIB)))
$(eval $(call EM_RECIPE_ADD_CLEANER,    clean-iconv))
########################################################################
# Export for php
########################################################################
export ICONV_DIR        = $(EM_ICONV_SRC)
export ICONV_LIBS       = $(EM_ICONV_LIB)
export ICONV_CFLAGS     = -I$(EM_ICONV_INC)
########################################################################
# Inject for autoconf
########################################################################
export php_cv_iconv_implementation="GNU libiconv"
export php_cv_iconv_errno=yes
export php_cv_iconv_ignore=yes
export ac_cv_func_iconv=yes
export ac_cv_func_libiconv=yes  
export ac_cv_lib_iconv_iconv=yes
export ac_cv_lib_iconv_libiconv=yes