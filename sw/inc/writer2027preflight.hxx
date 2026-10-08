/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SW_WRITER2027PREFLIGHT_HXX
#define INCLUDED_SW_WRITER2027PREFLIGHT_HXX

#include <rtl/ustring.hxx>

#include <vector>

class FontList;
class SwDoc;

/** Writer 2027 Publication Preflight (Phase 10).

    A deterministic, local, output-aware quality inspector for Writer
    documents. Independent from the UI: reusable by the Publication
    Inspector sidebar panel, the Publish dialog gate, and future headless/CI
    flows. Scanning is strictly read-only - no document mutation, no field
    refresh, no network, no macro execution.
 */
namespace sw::writer2027preflight
{
/** Stable, language-neutral severity. */
enum class PreflightSeverity
{
    Error,   // intended output is broken, unsafe, or loses essential content
    Warning, // output remains possible but materially degrades
    Info     // useful quality observation
};

/** Stable, language-neutral issue category (grouping). */
enum class PreflightCategory
{
    Document,
    Accessibility,
    Typography,
    Media,
    LinksReferences,
    Web,
    Pdf,
    Print
};

/** Output profiles. Checks declare profile relevance; a check that is
    irrelevant for the selected profile produces no issue. */
enum class PreflightProfile
{
    General,       // General Document
    PdfDigital,    // PDF - Digital Appearance
    PrintFriendly, // Physical Print - Print Friendly
    PrintDigital,  // Physical Print - Digital Appearance
    WebPackage,    // Web Package
    SingleHtml     // Single HTML
};

/** Scan mode: Quick is the interactive refresh (structure, fonts, links,
    basic accessibility, known unsupported objects); Full additionally runs
    the expensive checks (contrast, effective PPI, size estimate). */
enum class PreflightScanMode
{
    Quick,
    Full
};

/** Deterministic navigation target kind. */
enum class PreflightTargetKind
{
    None,
    TextPosition, // text node + range (mnNodeIndex/mnStart/mnEnd)
    Heading,      // heading text node
    Image,        // graphic fly frame (maObjectName = fly name)
    Frame,        // frame fly (maObjectName = fly name)
    Table,        // table (maObjectName = table name)
    Style,        // a named style
    Document      // document-level, no single object
};

/** Explicit safe fix entry point. Fixes only open canonical editing UI;
    nothing is changed automatically. */
enum class PreflightFixKind
{
    None,
    OpenImageProperties,  // image/frame title + description (alt text)
    OpenDocumentLanguage, // language and locale settings
    OpenParagraphStyle,   // paragraph style dialog
    OpenCharacterStyle,   // character style dialog
    OpenTypeSystem        // Writer 2027 Type System picker
};

/** One preflight finding. Identity is maCheckId (never localized text). */
struct PreflightIssue
{
    OUString maCheckId;           // stable id, e.g. "W27-A11Y-001"
    PreflightSeverity meSeverity = PreflightSeverity::Info;
    PreflightCategory meCategory = PreflightCategory::Document;
    OUString maTitle;             // localized short title
    OUString maDescription;       // localized plain explanation
    OUString maTargetId;          // deterministic target id, e.g. "image-3"
    PreflightTargetKind meTargetKind = PreflightTargetKind::None;
    sal_Int64 mnNodeIndex = -1;   // body node index for navigation (-1 = none)
    OUString maObjectName;        // fly/table name for GotoFly/GotoTable
    sal_Int32 mnStart = -1;       // text range start for navigation
    sal_Int32 mnEnd = -1;         // text range end for navigation
    PreflightFixKind meFixKind = PreflightFixKind::None;
    bool mbNavigable = false;
};

/** Engine options. */
struct PreflightOptions
{
    PreflightProfile meProfile = PreflightProfile::General;
    PreflightScanMode meMode = PreflightScanMode::Quick;
    const FontList* mpFontList = nullptr; // may be null (font checks degrade)
};

/** Deterministic scan result. */
struct PreflightReport
{
    OUString maProfile; // stable profile id string
    PreflightScanMode meMode = PreflightScanMode::Quick;
    sal_Int32 mnErrors = 0;
    sal_Int32 mnWarnings = 0;
    sal_Int32 mnInfos = 0;
    std::vector<PreflightIssue> maIssues; // scan order (deterministic)
};

/** Runs the preflight engine for one output profile. Read-only. */
PreflightReport RunPreflight(SwDoc& rDoc, const PreflightOptions& rOptions);

/** Deterministic JSON serialization of a report.

    No absolute paths, no secrets, no timestamps by default. Identity uses
    checkId, never localized message text. Issue order is the scan order.
 */
OUString PreflightReportToJson(const PreflightReport& rReport);

/** Stable profile id string for PreflightProfile (JSON + UI). */
const char* ProfileToString(PreflightProfile eProfile);

} // namespace sw::writer2027preflight

#endif // INCLUDED_SW_WRITER2027PREFLIGHT_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */