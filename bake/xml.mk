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
$(eval $(call EM_BAKE_RECIPE, libxml))
$(eval $(call EM_BAKE_RECIPE, xml))
$(eval $(call EM_BAKE_RECIPE, dom))
$(eval $(call EM_BAKE_RECIPE, simplexml))
$(eval $(call EM_BAKE_RECIPE, xmlreader))
$(eval $(call EM_BAKE_RECIPE, xmlwriter))