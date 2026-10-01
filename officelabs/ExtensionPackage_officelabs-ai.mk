# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# Bundle the OfficeLabs AI WriterAgent.oxt as a LibreOffice ExtensionPackage.
#
# gb_ExtensionPackage (solenv/gbuild/ExtensionPackage.mk) unpacks the OXT
# into $(INSTROOT)/$(LIBO_SHARE_FOLDER)/extensions/officelabs-ai and writes
# $(WORKDIR)/ExtensionPackage/officelabs-ai.filelist, which the scp2
# installer consumes (see scp2/source/extensions/file_extensions.scp,
# gid_File_Oxt_OfficeLabsAI).

$(eval $(call gb_ExtensionPackage_ExtensionPackage,officelabs-ai,\
	$(WORKDIR)/OfficeLabsAI/WriterAgent.oxt))

# The OXT is produced by the officelabs/writeragent-oxt custom target.
# Chain the ExtensionPackage preparation on the custom target's completion
# marker: the generic touch recipe in gb_ExtensionPackage then only
# timestamps the already-built archive instead of clobbering a build recipe.
$(call gb_ExtensionPackage_get_preparation_target,officelabs-ai) : \
	$(call gb_CustomTarget_get_target,officelabs/writeragent-oxt)

# vim: set noet sw=4 ts=4: