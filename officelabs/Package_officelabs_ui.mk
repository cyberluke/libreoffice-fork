# -*- Mode: makefile-gmake; tab-width: 4; indentents-mode: t -*-
#
# Deploy the OfficeLabs CEF web UI (officelabs/ui/officelabs-ui/) to
# instdir/program/officelabs-ui/. The UI is the CEF-side companion of the
# OfficeLabs sidebar; its AI actions/buttons use the TDesign AI icon set
# (assets/ai-*.svg, assets/robot*.svg) bundled with the UI.
#
# Only active when ENABLE_CEF=TRUE (--with-cef was specified).
#

ifeq ($(ENABLE_CEF),TRUE)

$(eval $(call gb_Package_Package,officelabs_ui,$(SRCDIR)/officelabs/ui))

$(eval $(call gb_Package_add_files,officelabs_ui,$(LIBO_BIN_FOLDER)/officelabs-ui,\
    officelabs-ui/index.html \
    officelabs-ui/assets/ai.svg \
    officelabs-ui/assets/ai-1.svg \
    officelabs-ui/assets/ai-article.svg \
    officelabs-ui/assets/ai-book-open.svg \
    officelabs-ui/assets/ai-chart-bar.svg \
    officelabs-ui/assets/ai-coordinate-system.svg \
    officelabs-ui/assets/ai-cut.svg \
    officelabs-ui/assets/ai-edit.svg \
    officelabs-ui/assets/ai-edit-1.svg \
    officelabs-ui/assets/ai-education.svg \
    officelabs-ui/assets/ai-git-branch.svg \
    officelabs-ui/assets/ai-image.svg \
    officelabs-ui/assets/ai-image-1.svg \
    officelabs-ui/assets/ai-layout.svg \
    officelabs-ui/assets/ai-music.svg \
    officelabs-ui/assets/ai-screenshot.svg \
    officelabs-ui/assets/ai-search.svg \
    officelabs-ui/assets/ai-terminal.svg \
    officelabs-ui/assets/ai-terminal-1.svg \
    officelabs-ui/assets/ai-textformat-italic.svg \
    officelabs-ui/assets/ai-tool.svg \
    officelabs-ui/assets/ai-video.svg \
    officelabs-ui/assets/robot.svg \
    officelabs-ui/assets/robot-1.svg \
    officelabs-ui/assets/robot-2.svg \
))

endif

# vim: set noet sw=4 ts=4: