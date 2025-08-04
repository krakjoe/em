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
$(eval $(call EM_RECIPE_ADD_CONFIGURE,  --enable-address-sanitizer))
$(eval $(call EM_RECIPE_ADD_CONFIGURE,  --enable-undefined-sanitizer))

$(eval $(call EM_RECIPE_ADD_CFLAGS,  -fno-omit-frame-pointer))
$(eval $(call EM_RECIPE_ADD_LDFLAGS, -fno-omit-frame-pointer))

$(eval $(call EM_RECIPE_ADD_CFLAGS,  -fsanitize=address))
$(eval $(call EM_RECIPE_ADD_CFLAGS,  -fsanitize=undefined))
$(eval $(call EM_RECIPE_ADD_LDFLAGS, -fsanitize=address))
$(eval $(call EM_RECIPE_ADD_LDFLAGS, -fsanitize=undefined))

$(eval $(call EM_RECIPE_ADD_CFLAGS,  -DEM_SANITIZERS))
$(eval $(call EM_RECIPE_ADD_LDFLAGS, -DEM_SANITIZERS))

