# Extra Modules

Contains templates, clipart galleries, palettes, symbol font, autocorrections, autotexts etc.

+ How-to add a new gallery:
	+ create a directory `extras/source/gallery/foo/`
	+ create a `.str` file `extras/source/gallery/foo/foo.str`
	+ add a `foo` section to `extras/source/gallery/share/gallery_names.ulf`
	+ add a `Gallery_foo.mk` at the top-level (and mention in `Module_extra.mk`)
	+ add a new `GALLERY_FILELIST` statement in `scp2/`

+ How-to add a new autotext category
	+ create a directory `extras/source/autotext/lang/xx/foo/` where `xx` is your lang code. `xx` must exactly fit with an UI lang code.
	+ unzip your `foo.bau` autotext file in this directory (excluding an empty mimetype file)
	+ add `xx/foo.bau` in `extras/AllLangPackage_autotextshare.mk`
	+ in `extras/CustomTarget_autotextshare.mk`:
		+ add all files contained in `foo.bau` (except mimetype) in `extras_AUTOTEXTSHARE_XMLFILES`

+ How-to add a new autotext to an existing category
	+ create a directory `extras/source/autotext/lang/xx/standard/FOO/` to add it in category standard of lang `xx`
	+ add files of the autotext (at least `FOO.xml` for an unformatted autotext)
	+ add autotext name in `extras/source/autotext/lang/xx/standard/BlockList.xml`
	+ add all files of autotext in `extras/source/autotext/lang/xx/standard/META-INF/manifest.xml`
	+ in `extras/CustomTarget_autotextshare.mk`:
		+ add all files of autotext in `extras_AUTOTEXTSHARE_XMLFILES`

+ How-to add a new template category
	+ add long category name in TEMPLATE_LONG_NAMES_ARY in sfx2/inc/doctempl.hrc
	+ add short category name in TEMPLATE_SHORT_NAMES_ARY in sfx2/source/doc/doctemplates.cxx

+ How-to add a new template to an existing category
	+ clean-up template file as indicated on wiki <https://wiki.documentfoundation.org/Documentation/HowTo/Impress/Make_template_language_independent>
	+ recommendation for settings.xml are given on wiki <https://wiki.documentfoundation.org/Documentation/CompatibilityFlags#Settings_to_include_in_a_new_template>
	+ add `<dc:title>Foo</dc:title>` in `meta.xml` to make presentation name translatable
	+ choose a template category `<Category>`
	+ unzip `Foo.ot?` (? = p, t, s or g) in `extras/source/templates/<Category>/Foo` (no space allowed in any file names)
	+ add `<Category>/Foo.ot? /` in `Package_templates.mk` (or `Package_tplpresnt.mk` if it's a presentation)
	+ in `template_files.mk`:
		+ add e.g. `<Category>/Foo/settings.xml` (and all other files which are not automatically added) to `extra_files_COMMON` (or `extra_files_PRESENTATIONS` in case of a presentation template)
+ for translation of template name
	+ define a new STR_TEMPLATE_NAMExx in include/sfx2/strings.hrc
	+ define a new STR_TEMPLATE_NAMExx_DEF in sfx2/inc/strings.hxx
	+ in sfx2/source/doc/doctempl.cxx
		+ add STR_TEMPLATE_NAMExx_DEF to aTemplateNames
		+ add STR_TEMPLATE_NAMExx to STR_TEMPLATE_NAME

## Third-party Writer templates (science and typography)

The Writer templates under `source/templates/science/` and `source/templates/typography/`
are third-party content, redistributed under the licenses below. The MIT copyright
notices are preserved here; the templates may also carry origin metadata in their
`meta.xml` files.

### `science/Science_Report.ott`, `science/Science_Short_Report.ott`,
### `science/Report_Cover_Minimal.ott` and `science/Report_Cover_Page.ott`

Derived from the Word templates in `AAGITemplates` (AAGI-reporting Word templates):

    MIT License
    Copyright (c) 2019 University of Adelaide Biometry Hub

The original templates embedded the AAGI logo and strategic-partner logos, which are
NOT covered by the MIT license and are retained by AAGI and its partners. Those logos
have been removed from the staged LibreOffice templates; the layout and styles are kept
and the institutional placeholder text was neutralised.

### `typography/Tech_Docs.ott` and `typography/Technical_Report_Callout.ott`

Derived from `libreoffice-writer-template` (Tech Docs Template and its accompanying
reference document showing the callout/style boxes):

    The MIT License (MIT)
    Copyright (c) 2014 Akbar Ahmed

### `typography/{Chancery,Meridian,Atlas,Foundry,Foundry_Plain}.ott`

Derived from `cellsummer/pandoc-word-templates` (pandoc reference docs, styles A–D).
The user holds a license grant for redistribution in this project. The embedded Word
OLE specimen object was removed during conversion (reference docs discard body content
on use; only styles/numbering/page-setup/headers are consumed).

### Impress templates under `source/templates/presnt/`

The `presnt/*.otp` templates prefixed with their collection (e.g. `apache-oo_*`,
`lo-cft_*`, `lo4-design-candidates_*`, `lo5-design-candidates_*`, `modern-impress-templates_*`,
`ubuntu-odp_*`, `kde-files-org_*`, `fedora-slideshow_*`, `chtsai-impress_*`,
`lo35-templates_*`, `lo51-templates_*`, `other_*`, `user-contrib_*`) come from the
`dohliam/libreoffice-impress-templates` curation of free-licensed LibreOffice Impress
templates (Apache OpenOffice templates, TDF Design-Team call-for-template candidates,
Ubuntu/Fedora/KDE sets, and community contributions).

Each template retains its original license, documented in its `<meta.xml>` and recorded in
the upstream collection's per-folder READMEs. Licenses in use: CC-BY, CC-BY-SA, GPL,
LGPL, MPL, Public Domain and MIT. The collection repository itself is MIT
(Copyright (c) 2015 dohliam). Templates released non-commercially (CC-BY-NC) have been
excluded from this build per their license terms.

Every template was converted to (or is a native) ODF Impress template and the result
verified to load and render in the runtime build.

Every template was converted from Word to ODF Writer via LibreOffice headless and the
result verified to load and render in the runtime build.
