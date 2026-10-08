/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SW_WRITER2027WEB_HXX
#define INCLUDED_SW_WRITER2027WEB_HXX

#include <rtl/ustring.hxx>

class SwDoc;

namespace sw::writer2027web
{

// Writer 2027 Web Publication (Phase 8).
//
// Exports the current Writer document model as a responsive, semantic web
// publication. Writer remains the authoring system: the exporter consumes
// the canonical document structures (paragraph/character styles, headings,
// lists, tables, sections, images, captions, links, bookmarks, footnotes,
// Phase 7 editorial blocks, Phase 6 typography) and produces plain
// HTML + CSS with no browser runtime and no second document model.

/** Output variant. */
enum class WebPublishFormat
{
    /** publication/ with index.html + styles/writer2027.css + assets/. */
    WebPackage,
    /** One index.html with the CSS inlined; images are inlined as data
        URIs when small, otherwise written as sibling assets. */
    SingleHtml,
};

/** Export options (Publish dialog). */
struct WebPublishOptions
{
    WebPublishFormat meFormat = WebPublishFormat::WebPackage;
    bool mbResponsive = true;       // responsive layout (clamp + breakpoints)
    bool mbIncludeMetadata = true;  // <title>, <meta author/description/keywords>
    bool mbIncludePrintCss = true;  // @media print block
    bool mbGenerateNav = false;     // derive a <nav> from real outline headings
    bool mbOptimizeImages = true;   // bounded responsive srcset variants (Web Package)
    bool mbEmbedFonts = false;      // @font-face: embed only fonts whose license permits it
};

/** Result of a publish run. */
struct WebPublishResult
{
    bool mbSuccess = false;
    OUString maIndexUrl; // file:// URL of the generated index.html
    OUString maError;    // user-readable error message (localized by caller)
};

/** Publish rDoc into rOutputDirUrl (a file:// directory URL, created if
    missing). Read-only with respect to the document: no content/style
    mutation, no modified flag, no undo entry. */
WebPublishResult PublishWeb(SwDoc& rDoc, const WebPublishOptions& rOptions,
                            const OUString& rOutputDirUrl);

/** Deterministic sanitizer for asset file names and HTML ids: keeps ASCII
    alphanumerics, '-' and '_', replaces everything else with '-'; result is
    lower-cased, trimmed and never empty (falls back to "item"). */
OUString SanitizeName(const OUString& rName);

} // namespace sw::writer2027web

#endif // INCLUDED_SW_WRITER2027WEB_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */