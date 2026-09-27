# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# Copy CEF runtime binaries and resources to instdir/program/
# Only active when ENABLE_CEF=TRUE (--with-cef was specified)
#

ifeq ($(ENABLE_CEF),TRUE)

# The 'cef' package is registered unconditionally in Repository.mk when
# ENABLE_CEF is set, so it must be DEFINED on every platform (an undefined but
# registered package that a module reads errors with "No target registered").
# On Windows it copies the loose CEF runtime DLLs next to soffice.exe. On macOS
# there are no loose runtime binaries -- the framework and the five Helper .app
# bundles are assembled and installed by CustomTarget_cef_mac_bundle -- so the
# package is defined empty (valid: FILES defaults to empty).
$(eval $(call gb_Package_Package,cef,$(CEF_DIR)/Release))

ifeq ($(OS),WNT)
# CEF Release DLLs -> instdir/program/
$(eval $(call gb_Package_add_files,cef,$(LIBO_BIN_FOLDER),\
    libcef.dll \
    chrome_elf.dll \
    d3dcompiler_47.dll \
    libEGL.dll \
    libGLESv2.dll \
    vk_swiftshader.dll \
    vulkan-1.dll \
    v8_context_snapshot.bin \
))
endif

endif

# vim: set noet sw=4 ts=4:
