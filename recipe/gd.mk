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
# We use bundled libgd, because the extension makes assumptions about
# the version of gd being used when forced to use an external libgd.
#
# The bundled version works fine, we just need to provide the deps ...
#
# There's a hard dependency on png, and optionally jpeg support will
# be included automatically where the jpeg recipe is built
########################################################################
# Setup recipe
########################################################################
$(eval $(call EM_RECIPE_ADD_CONFIGURE, --enable-gd))

$(eval $(call EM_RECIPE_ADD_DEP,       gd, $(EM_ZLIB_SRC)))
$(eval $(call EM_RECIPE_ADD_DEP,       gd, $(EM_ZLIB_INC)))
$(eval $(call EM_RECIPE_ADD_DEP,       gd, $(EM_ZLIB_LIB)))
$(eval $(call EM_RECIPE_ADD_DEP,       gd, $(EM_ZLIB_CFLAGS)))

$(eval $(call EM_RECIPE_ADD_DEP,       gd, $(EM_PNG_SRC)))
$(eval $(call EM_RECIPE_ADD_DEP,       gd, $(EM_PNG_INC)))
$(eval $(call EM_RECIPE_ADD_DEP,       gd, $(EM_PNG_LIB)))
$(eval $(call EM_RECIPE_ADD_DEP,       gd, $(EM_PNG_CFLAGS)))
########################################################################
# Export for php
########################################################################
export LIBPNG_LIBS   = $(EM_PNG_LIBS)
export LIBPNG_CFLAGS = $(EM_PNG_CFLAGS)

export LIBZ_LIBS     = $(EM_ZLIB_LIBS)
export LIBZ_CFLAGS   = $(EM_ZLIB_CFLAGS)