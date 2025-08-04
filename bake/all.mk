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
# Build the entire world ...
########################################################################
$(eval $(call EM_BAKE_RECIPE, bcmath))
$(eval $(call EM_BAKE_RECIPE, calendar))
$(eval $(call EM_BAKE_RECIPE, ctype))
$(eval $(call EM_BAKE_RECIPE, mbstring))
$(eval $(call EM_BAKE_RECIPE, opcache))
$(eval $(call EM_BAKE_RECIPE, tokenizer))
########################################################################
include $(EM_BAKE_IN)/compression.mk
include $(EM_BAKE_IN)/database.mk
include $(EM_BAKE_IN)/image.mk
include $(EM_BAKE_IN)/xml.mk