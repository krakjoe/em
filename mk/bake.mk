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
# Instead of requiring that all recipes are given to make, we can group
# recipes together and bake them at once.
#
# Bakers should be very simple files in all cases, only calling include
# and or EM_BAKE_RECIPE
#
#    EM_BAKE_RECIPE(recipe)
#	    Shall append the recipe to the recipe list if it is not excluded
#		using witout-% and not present on the command line (ie, it will
#		ignore duplicates).
#
########################################################################
EM_BAKE_IN     ?= $(EM_ROOT_DIR)/bake
########################################################################
# EM_BAKE_RECIPE
########################################################################
EM_BAKE_RECIPES ?=
define EM_BAKE_RECIPE
$(eval EM_BAKING_RECIPE := $(strip $(1)))
$(if $(EM_BAKING_RECIPE),
ifeq (,$(filter \
	without-$(EM_BAKING_RECIPE) \
	with-$(EM_BAKING_RECIPE),$(MAKECMDGOALS)))
ifeq (,$(filter \
	$(EM_BAKING_RECIPE),$(EM_BAKE_RECIPES)))
EM_BAKE_RECIPES += $(EM_BAKING_RECIPE)
endif
endif
)
endef