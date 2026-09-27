# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# CEF subprocess executable for multi-process architecture.
# Only built when ENABLE_CEF=TRUE (--with-cef was specified).
#

ifeq ($(ENABLE_CEF),TRUE)

$(eval $(call gb_Executable_Executable,officelabs_cef_subprocess))

$(eval $(call gb_Executable_set_include,officelabs_cef_subprocess,\
    -I$(CEF_DIR) \
    -I$(CEF_DIR)/include \
    $$(INCLUDE) \
))

$(eval $(call gb_Executable_add_defs,officelabs_cef_subprocess,\
    -DHAVE_FEATURE_CEF \
))

$(eval $(call gb_Executable_add_exception_objects,officelabs_cef_subprocess,\
    officelabs/source/cef_subprocess_main \
))

ifeq ($(OS),MACOSX)
# macOS helper: link the CEF C++ wrapper static lib (provides
# CefScopedLibraryLoader::LoadInHelper). The framework is loaded at runtime, not
# linked. This bare executable is copied+renamed into the five Helper .app
# bundles by CustomTarget_cef_mac_bundle.
$(eval $(call gb_Executable_add_libs,officelabs_cef_subprocess,\
    $(CEF_DIR)/build/libcef_dll_wrapper/libcef_dll_wrapper.a \
))
$(eval $(call gb_Executable_use_system_darwin_frameworks,officelabs_cef_subprocess,\
    AppKit \
    Cocoa \
    IOSurface \
))
else
$(eval $(call gb_Executable_add_defs,officelabs_cef_subprocess,\
    -DNOMINMAX \
    -DWIN32_LEAN_AND_MEAN \
))
$(eval $(call gb_Executable_add_libs,officelabs_cef_subprocess,\
    $(CEF_LIBS) \
    $(CEF_DIR)/libcef_dll_wrapper/Release/libcef_dll_wrapper.lib \
))
# Link as Windows GUI app (wWinMain entry point)
$(eval $(call gb_Executable_add_ldflags,officelabs_cef_subprocess,\
    /SUBSYSTEM:WINDOWS \
))
endif

endif

# vim: set noet sw=4 ts=4:
