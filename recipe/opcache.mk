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
# Note: opcache will compile and build and link, however,
#	**it won't actually function as a cache**.
# There are hard coded limitations in opcache that prevent it from
# working on virtual file systems and streams.
# The philosophy of em prohibits us from patching this source code,
# so there is nothing we are able to do about this ... for now ...
#
# It's possible to convince dmitry to add em to allowed sapis, but we
# need to work with him on that to make sure we actually can support
# opcache without compromising stability of em.   
########################################################################
# Version guard:
#  Pre 8.4, opcache would run AC_IF_ELSE during configure, we can't stop
#  that happening gracefully (by manipulating ac cache vars)
#  as a result it would break the build if enabled ...
########################################################################
ifeq ($(call EM_PHP_VERSION_GE,80400),true)
########################################################################
# Inject stub for build
########################################################################
$(eval $(call \
	EM_RECIPE_ADD_LINK_STUB,\
		$(EM_EXTRA_RECIPE_STUBS)/shm.c))
$(eval $(call \
	EM_RECIPE_ADD_LINK_STUB,\
		$(EM_EXTRA_RECIPE_STUBS)/initgroups.c))
########################################################################
# Inject for em
########################################################################
EM_EXTRA_CONFIGURE       += --enable-opcache
EM_EXTRA_CONFIGURE       += --disable-opcache-jit
########################################################################
# Inject for autoconf
########################################################################
export ac_cv_func_mprotect=yes
export php_cv_shm_ipc=yes
export php_cv_shm_mmap_anon=no
export php_cv_shm_mmap_posix=no
else
$(info Opcache will break the build <8.4, skipping ...)
endif