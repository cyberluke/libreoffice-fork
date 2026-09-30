# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# Copy CEF runtime RESOURCE files (ICU data, .pak bundles, locales) to
# instdir/program/. These live in the Resources/ directory of the CEF
# distribution, not Release/, and Chromium refuses to start without them
# ("Invalid file descriptor to ICU data received" when icudtl.dat is missing).
# Only active when ENABLE_CEF=TRUE (--with-cef was specified).
#
# The 'cef_res' package is registered in Repository.mk (gb_Helper_register_packages)
# and Module_officelabs.mk, so it must be DEFINED on every platform when
# ENABLE_CEF is set; on non-Windows the file list is empty (valid: FILES
# defaults to empty).

ifeq ($(ENABLE_CEF),TRUE)

$(eval $(call gb_Package_Package,cef_res,$(CEF_DIR)/Resources))

ifeq ($(OS),WNT)
# CEF Resources -> instdir/program/
$(eval $(call gb_Package_add_files,cef_res,$(LIBO_BIN_FOLDER),\
    icudtl.dat \
    resources.pak \
    chrome_100_percent.pak \
    chrome_200_percent.pak \
))
# Locale .pak files keep their subdirectory under program/.
$(eval $(call gb_Package_add_files_with_dir,cef_res,$(LIBO_BIN_FOLDER),\
    $(addprefix locales/,$(notdir $(wildcard $(CEF_DIR)/Resources/locales/*.pak))) \
))
endif

endif

# vim: set noet sw=4 ts=4: