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
# Shall define:
#  EM_PHP_VERSION from $(EM_PHP_DIR)/main/php_version.h
# Shall export:
#  EM_PHP_VERSION_GE - check if version is greater than or equal
#  EM_PHP_VERSION_GT - check if version is greater than
#  EM_PHP_VERSION_LT - check if version is less than
#  EM_PHP_VERSION_LE - check if version is less than or equal
# Example:
# ifeq ($(call EM_PHP_VERSION_GE,80400),true)
#	the rules in here are processed where PHP_VERSION_ID >= 80400
# endif
########################################################################
EM_PHP_VERSION := $(shell \
	awk '/PHP_VERSION_ID/ {print $$3}' \
		$(EM_PHP_DIR)/main/php_version.h)
define EM_PHP_VERSION_GE
$(shell test $(EM_PHP_VERSION) -ge $(1) && echo true || echo false)
endef

define EM_PHP_VERSION_GT
$(shell test $(EM_PHP_VERSION) -gt $(1) && echo true || echo false)
endef

define EM_PHP_VERSION_LT
$(shell test $(EM_PHP_VERSION) -lt $(1) && echo true || echo false)
endef

define EM_PHP_VERSION_LE
$(shell test $(EM_PHP_VERSION) -le $(1) && echo true || echo false)
endef