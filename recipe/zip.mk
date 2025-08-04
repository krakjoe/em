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
EM_ZIP_URL   = https://libzip.org/download/libzip-1.11.4.tar.xz
EM_ZIP_TAR   = $(EM_RECIPE_OUT)/libzip-1.11.4.tar.xz
EM_ZIP_SRC   = $(EM_RECIPE_OUT)/zip
EM_ZIP_LIB   = $(EM_ZIP_SRC)/lib/libzip.a
EM_ZIP_INC   = $(EM_ZIP_SRC)/include
########################################################################
EM_ZIP_OPTS      ?=
EM_ZIP_CFLAGS    ?=
EM_ZIP_LDFLAGS   ?=
EM_ZIP_CONFIGURE ?= --with-zip=static,$(EM_ZIP_SRC)
########################################################################
EM_ZIP_CONFIGURED  = $(EM_ZIP_SRC)/build/CMakeCache.txt
EM_ZIP_MADE        = $(EM_ZIP_SRC)/build/lib/libzip.a
########################################################################
.PHONY: clean-zip
########################################################################
$(EM_ZIP_TAR): $$(EM_ZLIB_LIB)
	wget $(EM_ZIP_URL) -O $(EM_ZIP_TAR)
	mkdir -p $(EM_ZIP_SRC)
	tar -C $(EM_ZIP_SRC) --strip-components=1 -xf \
		$(EM_ZIP_TAR)
	@touch $@

$(EM_ZIP_CONFIGURED): $(EM_ZIP_TAR)
	mkdir -p $(EM_ZIP_SRC)/build && \
		cd $(EM_ZIP_SRC)/build && \
			$(EMCMAKE) cmake .. \
				-DCMAKE_CROSSCOMPILING_EMULATOR="node" \
				-DCMAKE_INSTALL_PREFIX=$(EM_ZIP_SRC) \
				-DCMAKE_BUILD_TYPE=Release \
				-DBUILD_TOOLS=OFF \
				-DBUILD_SHARED_LIBS=OFF \
				-DENABLE_COMMONCRYPTO=OFF \
				-DENABLE_OPENSSL=OFF \
				-DZLIB_LIBRARY=$(EM_ZLIB_LIB) \
				-DZLIB_INCLUDE_DIR=$(EM_ZLIB_SRC)/include
	@touch $@

$(EM_ZIP_MADE): $(EM_ZIP_CONFIGURED)
	cd $(EM_ZIP_SRC)/build && \
	CFLAGS="$(EM_ZIP_CFLAGS)" LDFLAGS="$(EM_ZIP_LDFLAGS)" \
		$(EMMAKE) make -j$(EM_PHP_PROC)
	@touch $@

$(EM_ZIP_LIB): $(EM_ZIP_MADE)
	cd $(EM_ZIP_SRC)/build && \
	CFLAGS="$(EM_ZIP_CFLAGS)" LDFLAGS="$(EM_ZIP_LDFLAGS)" \
		$(EMMAKE) make install
	@touch $@

$(EM_RECIPE_STUBS)/zip_source_file_stdio_named.c.o: $(EM_RECIPE_STUBS)/zip_source_file_stdio_named.c $(EM_ZIP_LIB)
	$(CC) $(EM_PHP_CFLAGS) $(EM_RECIPE_CFLAGS) $(EM_EMSDK_CFLAGS) -I$(EM_ZIP_SRC)/include \
			-c $(EM_RECIPE_STUBS)/zip_source_file_stdio_named.c \
				-o $(EM_RECIPE_STUBS)/zip_source_file_stdio_named.c.o

rebuild-zip: $(EM_RECIPE_STUBS)/zip_source_file_stdio_named.c.o
	$(AR) rs $(EM_ZIP_LIB) \
		$(EM_RECIPE_STUBS)/zip_source_file_stdio_named.c.o

clean-zip:
	@rm -rf $(EM_ZIP_TAR)
	@rm -rf $(EM_ZIP_SRC)

########################################################################
# Setup recipe
########################################################################
$(eval $(call EM_RECIPE_ADD_CONFIGURE, $(EM_ZIP_CONFIGURE)))
$(eval $(call EM_RECIPE_ADD_TARGET,    $(EM_ZIP_MADE)))
$(eval $(call EM_RECIPE_ADD_LIB,       $(EM_ZIP_LIB)))
$(eval $(call EM_RECIPE_ADD_CLEANER,   clean-zip))
$(eval $(call EM_RECIPE_ADD_CFLAGS,    -DHAVE_EM_ZIP_VFS))
$(eval $(call EM_RECIPE_ADD_CFLAGS,    -I$(EM_ZIP_SRC)/build))
$(eval $(call EM_RECIPE_ADD_CFLAGS,    -I$(EM_ZIP_SRC)/include))
$(eval $(call EM_RECIPE_ADD_CFLAGS,    -I$(EM_ZIP_SRC)/lib))
$(eval $(call EM_RECIPE_ADD_DEP,       zip, EM_ZLIB_SRC))
$(eval $(call EM_RECIPE_ADD_DEP,       zip, EM_ZLIB_LIB))
$(eval $(call EM_RECIPE_ADD_BUILD_RULE,rebuild-zip))
########################################################################
# Export for php
########################################################################
export LIBZIP_LIBS       = $(EM_ZIP_LIB)
export LIBZIP_CFLAGS     = -I$(EM_ZIP_INC)
########################################################################
# Inject for autoconf
########################################################################
export ac_cv_lib_zip_zip_file_set_mtime=yes
export ac_cv_lib_zip_zip_file_set_encryption=yes
export ac_cv_lib_zip_zip_libzip_version=yes
export ac_cv_lib_zip_zip_register_progress_callback_with_state=yes
export ac_cv_lib_zip_zip_register_cancel_callback_with_state=yes
export ac_cv_lib_zip_zip_compression_method_supportedyes
