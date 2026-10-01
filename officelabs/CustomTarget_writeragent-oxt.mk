# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# Deterministic WriterAgent.oxt build for the bundled OfficeLabs AI
# extension.
#
# The OXT is assembled by officelabs/writeragent/officelabs-build/build-wa-oxt.sh
# from the vendored snapshot in officelabs/writeragent/ (pinned by
# officelabs/writeragent/UPSTREAM). The build never touches the network,
# never runs unopkg, never launches LibreOffice and never writes to $HOME.
#
# The registered target is a completion marker (writeragent-oxt.done); the
# OXT itself is emitted at $(WORKDIR)/OfficeLabsAI/WriterAgent.oxt and is
# consumed by ExtensionPackage_officelabs-ai.mk, whose generic touch recipe
# must not conflict with a real build recipe on the archive.

$(eval $(call gb_CustomTarget_CustomTarget,officelabs/writeragent-oxt))

$(eval $(call gb_CustomTarget_register_target,officelabs/writeragent-oxt,writeragent-oxt.done))

officelabs_writeragent_oxt_WORKDIR := $(gb_CustomTarget_workdir)/officelabs/writeragent-oxt
officelabs_writeragent_oxt_SRCDIR := $(SRCDIR)/officelabs/writeragent
officelabs_writeragent_oxt_PYTHON := $(call gb_ExternalExecutable_get_command,python)
officelabs_writeragent_oxt_OXT := $(WORKDIR)/OfficeLabsAI/WriterAgent.oxt

# Any change to the vendored source, the OfficeLabs overlays or the build
# script rebuilds the OXT. vendor/ and build-tools/ are excluded from the
# scan (they only change through bin/update-writeragent-upstream.py).
officelabs_writeragent_oxt_SOURCES := \
	$(shell find $(officelabs_writeragent_oxt_SRCDIR) -type f \
		! -path '*/vendor/*' ! -path '*/build-tools/*')

$(officelabs_writeragent_oxt_WORKDIR)/writeragent-oxt.done : \
	$(officelabs_writeragent_oxt_SOURCES) \
	$(officelabs_writeragent_oxt_SRCDIR)/officelabs-build/build-wa-oxt.sh \
	$(call gb_Package_get_target,python3)
	$(call gb_Output_announce,officelabs/writeragent-oxt,$(true),CUS,3)
	$(call gb_Helper_abbreviate_dirs,\
		PATH="$(call gb_Helper_cyg_path,$(INSTDIR_FOR_BUILD)/$(LIBO_URE_LIB_FOLDER):$(INSTDIR_FOR_BUILD)/$(LIBO_BIN_FOLDER)):$$PATH" \
			bash $(officelabs_writeragent_oxt_SRCDIR)/officelabs-build/build-wa-oxt.sh \
				$(officelabs_writeragent_oxt_SRCDIR) \
				$(officelabs_writeragent_oxt_WORKDIR) \
				$(officelabs_writeragent_oxt_OXT) \
				python)
	$(call gb_Helper_make_zip_deterministic,$(officelabs_writeragent_oxt_OXT))
	touch $@

# vim: set noet sw=4 ts=4: