# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# Assemble and install the macOS CEF payload into the app bundle's
# Contents/Frameworks:
#   * "Chromium Embedded Framework.framework" (rebuilt into the versioned
#     Versions/A + symlink layout, mirroring cefsimple's cmake
#     COPY_MAC_FRAMEWORK so the embedded framework is codesign-valid), and
#   * the five Helper .app bundles (main/GPU/Renderer/Plugin/Alerts), each
#     built from the single officelabs_cef_subprocess binary + its Info.plist
#     template in officelabs/mac/helper-plists/.
#
# Why a CustomTarget instead of a Package/GeneratedPackage: every path here
# contains spaces ("Chromium Embedded Framework.framework", "OfficeLabs
# Helper (GPU).app", ...). gbuild's Package list macros word-split on spaces
# via $(foreach), so they cannot express these names. All space handling is
# therefore done in the shell recipe with quoting. Added to the module only
# on macOS + ENABLE_CEF (see Module_officelabs.mk).
#
# Delivery note (for review): this recipe installs directly into $(INSTROOT)
# rather than through a tracked Package, so `make clean` will not remove the
# payload from instdir and a wiped-but-not-rebuilt instdir is not
# re-detected. The CEF build is always an incremental rebuild on top of a
# completed base build (Contents/Frameworks already exists), which is the
# assumed ordering.

$(eval $(call gb_CustomTarget_CustomTarget,officelabs/cef_mac_bundle))

$(eval $(call gb_CustomTarget_register_target,officelabs/cef_mac_bundle,cef_mac_bundle.done))

officelabs_cef_mac_WORKDIR := $(gb_CustomTarget_workdir)/officelabs/cef_mac_bundle
officelabs_cef_mac_STAGE := $(officelabs_cef_mac_WORKDIR)/staging
officelabs_cef_mac_INSTDIR := $(INSTROOT)/$(LIBO_LIB_FOLDER)
officelabs_cef_mac_CEFRELEASE := $(CEF_DIR)/Release
officelabs_cef_mac_PLISTDIR := $(SRCDIR)/officelabs/mac/helper-plists
officelabs_cef_mac_SUBPROC := $(call gb_Executable_get_target,officelabs_cef_subprocess)

# Prerequisites are all space-free paths (the built helper binary + the five
# plist templates). The framework source path contains spaces and therefore
# cannot be a make prerequisite; it is consumed inside the recipe only.
$(officelabs_cef_mac_WORKDIR)/cef_mac_bundle.done : \
	$(officelabs_cef_mac_SUBPROC) \
	$(officelabs_cef_mac_PLISTDIR)/helper.plist \
	$(officelabs_cef_mac_PLISTDIR)/helper-gpu.plist \
	$(officelabs_cef_mac_PLISTDIR)/helper-renderer.plist \
	$(officelabs_cef_mac_PLISTDIR)/helper-plugin.plist \
	$(officelabs_cef_mac_PLISTDIR)/helper-alerts.plist
	$(call gb_Output_announce,officelabs/cef_mac_bundle,$(true),CUS,3)
	rm -rf "$(officelabs_cef_mac_STAGE)"
	mkdir -p "$(officelabs_cef_mac_STAGE)/Contents/Frameworks"
	set -e; \
	fw="$(officelabs_cef_mac_STAGE)/Contents/Frameworks/Chromium Embedded Framework.framework"; \
	mkdir -p "$${fw}/Versions"; \
	ditto "$(officelabs_cef_mac_CEFRELEASE)/Chromium Embedded Framework.framework" "$${fw}/Versions/A"; \
	ln -sfn "A" "$${fw}/Versions/Current"; \
	ln -sfn "Versions/Current/Chromium Embedded Framework" "$${fw}/Chromium Embedded Framework"; \
	ln -sfn "Versions/Current/Libraries" "$${fw}/Libraries"; \
	ln -sfn "Versions/Current/Resources" "$${fw}/Resources"
	set -e; \
	for entry in \
	"OfficeLabs Helper|helper.plist" \
	"OfficeLabs Helper (GPU)|helper-gpu.plist" \
	"OfficeLabs Helper (Renderer)|helper-renderer.plist" \
	"OfficeLabs Helper (Plugin)|helper-plugin.plist" \
	"OfficeLabs Helper (Alerts)|helper-alerts.plist"; do \
	    name="$${entry%%|*}"; plist="$${entry##*|}"; \
	    app="$(officelabs_cef_mac_STAGE)/Contents/Frameworks/$${name}.app"; \
	    mkdir -p "$${app}/Contents/MacOS"; \
	    cp "$(officelabs_cef_mac_PLISTDIR)/$${plist}" "$${app}/Contents/Info.plist"; \
	    cp "$(officelabs_cef_mac_SUBPROC)" "$${app}/Contents/MacOS/$${name}"; \
	    chmod 755 "$${app}/Contents/MacOS/$${name}"; \
	done
	mkdir -p "$(officelabs_cef_mac_INSTDIR)"
	set -e; \
	for item in \
	"Chromium Embedded Framework.framework" \
	"OfficeLabs Helper.app" \
	"OfficeLabs Helper (GPU).app" \
	"OfficeLabs Helper (Renderer).app" \
	"OfficeLabs Helper (Plugin).app" \
	"OfficeLabs Helper (Alerts).app"; do \
	    rm -rf "$(officelabs_cef_mac_INSTDIR)/$${item}"; \
	    ditto "$(officelabs_cef_mac_STAGE)/Contents/Frameworks/$${item}" "$(officelabs_cef_mac_INSTDIR)/$${item}"; \
	done
	touch $@

# vim: set noet sw=4 ts=4:
