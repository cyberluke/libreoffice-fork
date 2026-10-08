/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SW_INC_WRITER2027VIEW_HXX
#define INCLUDED_SW_INC_WRITER2027VIEW_HXX

#include <tools/color.hxx>
#include <swtypes.hxx>

#include <optional>

class SwDoc;
class SwViewShell;

namespace sw::writer2027view
{
// Writer 2027 — digital canvas presentation (Phase 4).
//
// These colors are *application/editor presentation* only: they style the
// workspace around the document stage and are never written into the
// document. The page itself keeps its real stored formatting (Phase 1 page
// colors, or any imported document's own colors). Light UI themes and
// high-contrast mode are not affected (see IsWriter2027CanvasActive).

/// Dark-first workspace around the document stage.
constexpr Color WorkspaceBackground(0x09, 0x0D, 0x12);
/// Restrained 1 px page boundary (subtle blue-gray on the dark workspace).
constexpr Color PageBoundary(0x2A, 0x33, 0x3C);
/// Minimal elevation: near-black shadow instead of the classic paper glow.
constexpr Color PageShadow(0x00, 0x00, 0x00);

/// true when the UI theme is dark (and not high contrast).
bool IsDarkThemeActive();

/// true when the Standard page style of the document carries the Writer 2027
/// digital page background (see sw/inc/writer2027.hxx). Imported documents
/// are never affected.
bool IsWriter2027DarkDocument(SwDoc& rDoc);

/// true when the Writer 2027 canvas presentation should be active:
/// dark UI theme AND Writer 2027 dark document.
bool IsWriter2027CanvasActive(SwDoc& rDoc);

/// Golden-ratio-inspired horizontal gutter (twips, per side) around the
/// document stage.
///
/// On wide windows the page keeps ~62% of the stage width (breathing room,
/// not maximum magnification); between ~1267 px and ~1667 px of edit-window
/// width the gutter ramps in; narrower windows keep the classic 5 mm border
/// so the existing functional fit behavior wins.
SwTwips ComputeCanvasBorder(tools::Long nViewWidth);

/// Gap between pages (twips) for the Writer 2027 canvas: slightly larger on
/// 4K-class windows so pages never touch, never below the classic 5 mm gap.
SwTwips ComputeCanvasGap(tools::Long nViewWidth);

/// Authoring modes. All modes edit the same Writer document through the
/// existing view/layout option machinery; none of them mutate content.
enum class AuthoringMode
{
    Layout, ///< classic paginated print layout (normal view)
    Flow,   ///< continuous editing view (draft view)
    Story,  ///< writing-first continuous text view (draft + reduced chrome)
    Focus,  ///< reduced application chrome (fullscreen)
    Web,    ///< web/browse layout
};

/// Map the current view-option state onto an authoring mode.
AuthoringMode GetCurrentAuthoringMode(const SwViewShell& rSh);

// ---------------------------------------------------------------------------
// Phase 5 — digital output intent.
//
// A Writer document has two output intentions:
//   * DigitalAppearance — preserve the authored appearance (dark page stays
//     dark, light text stays light). This is the PDF-export default.
//   * PrintFriendly    — non-destructive rendering transform for paper:
//     white page, dark text, reduced ink. This is the physical-print default
//     for Writer 2027 dark documents.
//
// The intent is a *job/session* property. It is resolved from the print or
// PDF dialog choice (or the job default) in SwRenderData::MakeSwPrtOptions
// and carried through SwPrintData; it never touches the document content.
// ---------------------------------------------------------------------------

/// Output intent of one print/export job.
///
/// The numeric values are the transport values used by the print dialog and
/// the PDF export dialog ("Writer2027OutputIntent" / "Writer2027PDFOutputIntent"
/// UI properties): 0 = PrintFriendly, 1 = DigitalAppearance.
enum class WriterOutputIntent
{
    PrintFriendly = 0,    ///< white paper, dark text, reduced ink
    DigitalAppearance = 1 ///< preserve document colors and page background
};

/// Default intent for a job that carries no explicit user choice.
WriterOutputIntent DefaultOutputIntent(bool bIsPDFExport, bool bIsWriter2027DarkDocument);

/// Map a color onto its paper-safe counterpart, but ONLY when it is one of
/// the Writer 2027 default digital-palette colors (see sw/inc/writer2027.hxx).
/// Arbitrary user colors are deliberately left untouched. Returns no value
/// when the color is not part of the default palette.
std::optional<Color> GetPrintFriendlyColor(const Color& rColor);

// ---------------------------------------------------------------------------
// Phase 2 — adaptive authoring optics (screen-first).
//
// Writer 2027 must not default to a physically literal, visually tiny 100%
// page on a high-resolution display. The initial authoring zoom is derived
// from real geometry (page width, available document-stage width, display
// scale) so normal body type reads substantially larger than legacy Writer
// 100% on 4K — approximately 1.6x-2.2x the legacy glyph scale — without ever
// altering document metrics (the page keeps its real 11/12 pt type; this is
// view optics only).
//
// This is an INITIAL/default presentation policy. The user's own zoom choice
// always wins afterwards (see ComputeInitialAuthoringZoom's callers: it must
// only be applied on new-document initial view / "Optimal Authoring").
// ---------------------------------------------------------------------------

/// Derive the initial authoring zoom (in percent) for a Writer 2027 document.
///
/// Inputs (both twips):
///   nPageWidthTwips    — physical page width, e.g. A4 ≈ 11906.
///   nStageWidthTwips   — available document-stage width.
///
/// Policy: zoom so the page occupies the golden-ratio stage share (~61.8%).
/// The ratio is unit-consistent, so on a wide 4K stage the page zooms up
/// substantially (1.6x-2.2x the legacy 100% glyph scale) while a compact
/// window stays near legacy. Clamped into a sane authoring band (100%-260%).
sal_uInt16 ComputeInitialAuthoringZoom(tools::Long nPageWidthTwips,
                                       tools::Long nStageWidthTwips);
}

#endif // INCLUDED_SW_INC_WRITER2027VIEW_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */