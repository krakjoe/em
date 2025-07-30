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
# Shall define vars for recipe integration and export macros for use in
# recipe files.
#
# Exported Variables:
#  EM_RECIPE_IN    - the path to recipes
#  EM_RECIPE_STUBS - the path to recipe stub files
#  EM_RECIPE_OUT   - the path to recipe working directory
#
# Exported Macros:
#   EM_RECIPE_ADD_CONFIGURE(--with|--enable|etc)
#     Shall append configuration, ignoring duplicates
#
#   EM_RECIPE_ADD_TARGET(rule)
#	  Shall append the target rule to recipe target rules
#	  Shall ignore duplicates
#	  Recipe target are processed before the build
#		(ie, configure depends on recipe target rules)
#
#   EM_RECIPE_ADD_CLEANER(rule)
#	  Shall append the cleaner rule to recipe cleaner rule
#	  Shall ignore duplicates
#	  !Recipes need to cleanup after themselves during a make clean!
#
#   EM_RECIPE_ADD_BUILD(source)
#     Shall compile source (full path, use EM_RECIPE_STUBS) in time for
#     build (ie, pre configure)
#     This should be used where a stub is required for PHP to configure
#	  Will ignore duplicates
#  
#   EM_RECIPE_ADD_LINK(source)
#     Shall compoile source (full path, use EM_RECIPE_STUBS) in time for
#     link (ie, post build)
#     This should be used where a stub is required for the final link
#	  Will ignore duplicates
#
#   EM_RECIPE_ADD_CFLAGS(flags)
#     Shall append flags to the CFLAGS used while compiling (stubs)
#
#   EM_RECIPE_ADD_LDFLAGS(flags)
#     Shall append flags to the LDFLAGS used for final link
#
#   EM_RECIPE_ADD_LIB(lib)
#     Shall append the library to recipe library list
#	  This should be used to add a library to the final link command
#     Will ignore duplicates
#
########################################################################
# Public, may be set by caller
########################################################################
EM_RECIPE_IN       ?= $(EM_ROOT_DIR)/recipe
EM_RECIPE_STUBS    ?= $(EM_RECIPE_IN)/stubs
EM_RECIPE_OUT      ?= /tmp
########################################################################
# EM_RECIPE_ADD_CONFIGURE
########################################################################
EM_RECIPE_CONFIGURE  ?=
define EM_RECIPE_ADD_CONFIGURE
ifeq ($(filter $(1),$(EM_RECIPE_CONFIGURE)),)
EM_RECIPE_CONFIGURE  += $(1)
endif
endef
########################################################################
# EM_RECIPE_ADD_TARGET
########################################################################
EM_RECIPE_TARGETS  ?=
define EM_RECIPE_ADD_TARGET
ifeq ($(filter $(1),$(EM_RECIPE_TARGETS)),)
EM_RECIPE_TARGETS  += $(1)
endif
endef
########################################################################
# EM_RECIPE_ADD_CLEANER
########################################################################
EM_RECIPE_CLEANERS ?=
define EM_RECIPE_ADD_CLEANER
ifeq ($(filter $(1),$(EM_RECIPE_CLEANERS)),)
EM_RECIPE_CLEANERS  += $(1)
endif
endef
########################################################################
# EM_RECIPE_ADD_BUILD
########################################################################
EM_RECIPE_BUILD_SOURCE  ?=
EM_RECIPE_BUILD_OBJECTS ?=
define EM_RECIPE_ADD_BUILD
ifeq ($(filter $(1),$(EM_RECIPE_BUILD_SOURCE)),)
EM_RECIPE_BUILD_SOURCE  += $(1)
EM_RECIPE_BUILD_OBJECTS += $(1:.c=.lo)
endif
endef
########################################################################
# EM_RECIPE_ADD_LINK
########################################################################
EM_RECIPE_LINK_SOURCE   ?=
EM_RECIPE_LINK_OBJECTS  ?=
define EM_RECIPE_ADD_LINK
ifeq ($(filter $(1),$(EM_RECIPE_LINK_SOURCE)),)
EM_RECIPE_LINK_SOURCE  += $(1)
EM_RECIPE_LINK_OBJECTS += $(1:.c=.lo)
endif
endef
########################################################################
# EM_RECIPE_ADD_LIB
########################################################################
EM_RECIPE_LIBS  ?=
define EM_RECIPE_ADD_LIB
ifeq ($(filter $(1),$(EM_RECIPE_LIBS)),)
EM_RECIPE_LIBS  += $(1)
endif
endef
########################################################################
# EM_RECIPE_ADD_CFLAGS
########################################################################
EM_RECIPE_CFLAGS  ?=
define EM_RECIPE_ADD_CFLAGS
ifeq ($(filter $(1),$(EM_RECIPE_CFLAGS)),)
EM_RECIPE_CFLAGS  += $(1)
endif
endef
########################################################################
# EM_RECIPE_ADD_LDFLAGS
########################################################################
EM_RECIPE_LDFLAGS  ?=
define EM_RECIPE_ADD_LDFLAGS
ifeq ($(filter $(1),$(EM_RECIPE_LDFLAGS)),)
EM_RECIPE_LDFLAGS  += $(1)
endif
endef
########################################################################