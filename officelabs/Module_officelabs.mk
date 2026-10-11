# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-

$(eval $(call gb_Module_Module,officelabs))

$(eval $(call gb_Module_add_targets,officelabs,\
    Library_officelabs \
    UIConfig_officelabs \
))

# Bundled WriterAgent ("OfficeLabs AI") — enabled through
# --enable-ext-officelabs-ai (adds OFFICELABS_AI to BUILD_TYPE; see
# configure.ac). The custom target produces WriterAgent.oxt from the
# vendored snapshot and the ExtensionPackage stages it into
# instdir/share/extensions/officelabs-ai for clean-profile registration.
ifeq ($(filter OFFICELABS_AI,$(BUILD_TYPE)),OFFICELABS_AI)
$(eval $(call gb_Module_add_targets,officelabs,\
    CustomTarget_writeragent-oxt \
    ExtensionPackage_officelabs-ai \
))
endif

$(eval $(call gb_Module_add_check_targets,officelabs,\
	CppunitTest_officelabs_inline \
	CppunitTest_officelabs_cursor \
	CppunitTest_officelabs_job \
))

# CEF WebView support (conditional on --with-cef)
ifeq ($(ENABLE_CEF),TRUE)
$(eval $(call gb_Module_add_check_targets,officelabs,\
	CppunitTest_officelabs_controller \
))

$(eval $(call gb_Module_add_targets,officelabs,\
    Executable_officelabs_cef_subprocess \
    Package_cef \
    Package_cef_res \
    Package_officelabs_ui \
))
# macOS: assemble + install the CEF framework and the five Helper .app bundles
# into Contents/Frameworks. Their paths contain spaces, which gbuild's Package
# list macros cannot express, so a CustomTarget does the space-safe shell copy.
ifeq ($(OS),MACOSX)
$(eval $(call gb_Module_add_targets,officelabs,\
    CustomTarget_cef_mac_bundle \
))
endif
endif

# vim: set noet sw=4 ts=4:
