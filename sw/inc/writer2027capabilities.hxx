/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SW_WRITER2027CAPABILITIES_HXX
#define INCLUDED_SW_WRITER2027CAPABILITIES_HXX

#include <rtl/ustring.hxx>
#include <svx/swframetypes.hxx>
#include <vcl/gfxlink.hxx>

#include <vector>

/** Pure publication-capability helpers shared by the Phase 9 web exporter and
    the Phase 10 preflight engine.

    One capability model: preflight must never predict exporter behavior with
    a divergent ruleset, and the exporter must never change its output
    semantics while these helpers are extracted.
 */
namespace sw::writer2027capabilities
{
/** URL scheme allow-list.

    Returns the URL unchanged when it is safe for a web publication (relative
    paths, fragment-only anchors, root-relative paths, http/https/mailto/tel)
    and an empty string for executable/unsafe schemes (javascript:, vbscript:,
    data:text/html, ...).
 */
OUString SafeHref(const OUString& rUrl);

/** Maps a native GfxLink type to a browser-usable asset (extension + mime).

    Returns false for formats that must not be shipped to a web page as-is
    (TIFF, WMF/EMF, EPS, PDF, MOV, ...).
 */
bool GfxTypeToWeb(::GfxLinkType eType, OUString& rExt, OUString& rMime);

/** Conservative SVG safety check.

    Rejects scripts, event-handler attributes, foreignObject and external
    (non-fragment) references so no ad-hoc sanitizer is needed. Returns false
    for data that must be rasterized or omitted instead of embedded raw.
 */
bool SvgIsSafe(const std::vector<sal_uInt8>& rData);

/** Web export capability classification of a frame anchor. */
enum class FrameCapability
{
    Full,    // content is exported with full fidelity
    Degraded, // content is exported in a reduced form
    Omitted  // content is not exported to the web publication
};

FrameCapability ClassifyFrameAnchor(::RndStdIds eAnchor);

} // namespace sw::writer2027capabilities

#endif // INCLUDED_SW_WRITER2027CAPABILITIES_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */