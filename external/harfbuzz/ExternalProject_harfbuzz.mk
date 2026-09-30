# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_ExternalProject_ExternalProject,harfbuzz))

$(eval $(call gb_ExternalProject_register_targets,harfbuzz,\
	build \
))

$(eval $(call gb_ExternalProject_use_externals,harfbuzz,\
	icu \
	graphite \
	meson \
))

# Hide HarfBuzz symbols.
harfbuzz_cpp_args = $(CXXFLAGS) $(if $(filter-out MSC,$(COM)),-fvisibility=hidden) \
	$(if $(ENABLE_DBGUTIL)$(ENABLE_DEBUG),,$(if $(call gb_Module__symbols_enabled,harfbuzz),$(gb_DEBUGINFO_FLAGS)))

# We cannot use environment vars inside the meson cross-build file,
# so we're going to have to generate one on-the-fly.
# mungle variables into python list format
python_listify = '$(subst $(WHITESPACE),'$(COMMA)',$(strip $(1)))'
# Convert cygwin paths to Windows paths for meson on WNT
ifeq ($(OS),WNT)
cross_c = $(call python_listify,$(shell cygpath -m '$(gb_CC)'))
cross_cxx = $(call python_listify,$(shell cygpath -m '$(gb_CXX)'))
else
cross_c = $(call python_listify,$(gb_CC))
cross_cxx = $(call python_listify,$(gb_CXX))
endif
cross_ld := $(call python_listify,$(subst -fuse-ld=,,$(USE_LD)))

harfbuzz_pkgconfig_sep := $(if $(filter WNT,$(OS)),;,$(LIBO_PATH_SEPARATOR))

define gb_harfbuzz_cross_compile
[binaries]
c = [$(cross_c)]
cpp = [$(cross_cxx)]
c_ld = [$(cross_ld)]
cpp_ld = [$(cross_ld)]
ar = '$(AR)'
strip = '$(STRIP)'
# LOCAL (V271): in a meson cross build, the pkg-config binary must come from
# the cross file (meson refuses PATH search with allow_default_for_cross=False).
# Without this entry meson reports "Pkg-config binary missing from cross or
# native file" and harfbuzz's graphite2 dependency cannot be resolved. Points
# at the standalone native pkgconf installed next to the workdir junction
# (accepts ';' PKG_CONFIG_PATH like the recipe builds it).
pkgconfig = 'C:/lo-build/pkgconf-2.4.3.exe'
# TODO: this is pretty ugly...
[host_machine]
system = '$(if $(filter WNT,$(OS)),windows,$(if $(filter MACOSX,$(OS)),darwin,$(if $(filter ANDROID,$(OS)),android,linux)))'
cpu_family = '$(subst X86_64,x86_64,$(RTL_ARCH))'
cpu = '$(if $(filter x86,$(RTL_ARCH)),i686,$(if $(filter X86_64,$(RTL_ARCH)),x86_64,$(if $(filter AARCH64,$(RTL_ARCH)),aarch64,armv7)))'
endian = '$(ENDIANNESS)'
endef

# cannot use CROSS_COMPILING as condition since we have cross-compilation "light" for cases where
# the builder can run the host binaries, like for example when compiling for win 32bit on win 64bit
#
#TODO: Drop filtering out of -std=c++29 and -std=c++2d once Meson supports C++29:
$(call gb_ExternalProject_get_state_target,harfbuzz,build) : | $(call gb_ExternalExecutable_get_dependencies,python)
	$(call gb_Trace_StartRange,harfbuzz,EXTERNAL)
	$(file >$(gb_UnpackedTarball_workdir)/harfbuzz/cross-file.txt,$(gb_harfbuzz_cross_compile))
	cp -f $(gb_UnpackedTarball_workdir)/graphite/graphite2-uninstalled.pc $(gb_UnpackedTarball_workdir)/graphite/graphite2.pc 2>/dev/null || true
	# LOCAL (V271): SOLARINC (MSVC/UCRT/SDK -I dirs, 8.3 short form) is folded
	# into c_args/cpp_args: meson applies the recipe INCLUDE env to its
	# configure-time compiler checks but not to the ninja-built objects (cl then
	# fails C1083 inttypes.h). As -I args in c_args/cpp_args the dirs reach
	# every cl invocation.
	$(call gb_ExternalProject_run,build,\
		PKG_CONFIG_PATH="$${PKG_CONFIG_PATH:+$${PKG_CONFIG_PATH}$(harfbuzz_pkgconfig_sep)}$(gb_UnpackedTarball_workdir)/graphite$(if $(SYSTEM_ICU),,$(harfbuzz_pkgconfig_sep)$(gb_UnpackedTarball_workdir)/icu)" \
		PYTHONWARNINGS= \
		INCLUDE="$(gb_ExternalProject_INCLUDE)" \
		$(MESON) setup --wrap-mode nofallback builddir \
			-Ddefault_library=static -Dbuildtype=$(if $(ENABLE_DBGUTIL),debug,$(if $(ENABLE_DEBUG),debugoptimized,release)) \
			$(addsuffix "$(strip $(SOLARINC) $(harfbuzz_cpp_args))",-Dc_args= -Dcpp_args=) \
			-Dauto_features=disabled \
			-Dcpp_std=$(subst c++29,c++26,$(subst c++2d,c++26,$(subst -std:,,$(subst -std=,,$(filter -std%,$(CXXFLAGS_CXX11)))))) \
			-Dtests=disabled \
			-Dutilities=disabled \
			-Dsubset=enabled \
			-Draster=disabled \
			-Dvector=disabled \
			-Dicu=enabled \
			-Dicu_builtin=true \
			-Dgraphite2=enabled \
			$(if $(filter MSC_TRUE,$(COM)_$(MSVC_USE_DEBUG_RUNTIME)),-Db_vscrt=mdd) \
			$(if $(filter-out $(BUILD_PLATFORM),$(HOST_PLATFORM))$(WSL)$(filter WNT,$(OS)),--cross-file cross-file.txt) && \
		$(MESON) compile -C builddir libs \
			$(if $(verbose),--verbose) \
	)
	$(call gb_Trace_EndRange,harfbuzz,EXTERNAL)

# vim: set noet sw=4 ts=4:
