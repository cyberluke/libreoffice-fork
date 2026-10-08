/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SVX_WRITER2027BLOCKS_HXX
#define INCLUDED_SVX_WRITER2027BLOCKS_HXX

#include <rtl/ustring.hxx>
#include <sal/types.h>

#include <unotools/resmgr.hxx>

#include <vector>

namespace svx::writer2027
{

// Writer 2027 Document Kits and Editorial Blocks (Phase 7).
//
// A Document Kit is a *composition* system: page geometry / section rhythm /
// recommended editorial blocks / a preferred (but not locked) Type System.
// An Editorial Block is a reusable structural composition that is inserted
// into the actual Writer document from canonical Writer primitives only.
//
// These catalogs are pure data, mirroring the Phase 6 Type System catalog:
// independent from the UI and from the installed-font enumeration. The actual
// Writer document structures and styles remain authoritative - a kit/block
// never serializes itself into the document.

/** Stable block ids. The insertion service switches on these. */
enum class EditorialBlockId
{
    Hero,
    SectionIntro,
    PullQuote,
    KeyStat,
    ImageCaption,
    TwoColumn,
    Callout,
    ResearchNote,
    Code,
    ClosingStatement,
};

/** Gallery grouping. "Recommended" is the top group and is ordered by the
    kit that is currently "active" in the session (see §27: this is a
    session-level insertion aid, never a whole-document match). */
enum class EditorialBlockCategory
{
    Recommended, // kit-recommended blocks; falls back to all when no kit is active
    Editorial,
    Technical,
};

/** How a block enters the document flow (see §13). */
enum class EditorialBlockInsertionPolicy
{
    AtCursor,             // insert directly at the cursor position
    NewParagraph,         // start on a fresh paragraph (split a non-empty one)
    NewSection,           // wrap the block content in a bounded section
    NewPage,              // start the block on a new page
    ReplaceEmptyDocument  // reuse the single empty paragraph of a new document
};

/** One reusable Editorial Block definition. Pure data: describes how
    canonical Writer objects are created (paragraphs, styles, sections,
    frames). No opaque binary fragments are stored. */
struct EditorialBlockDefinition
{
    OUString maId;                  // stable id, e.g. "hero"
    TranslateId maNameResId;        // localized display name (svx/inc/strings.hrc, NC_)
    TranslateId maDescriptionResId; // one-line localized description
    EditorialBlockCategory meCategory = EditorialBlockCategory::Recommended;
    EditorialBlockInsertionPolicy meInsertionPolicy = EditorialBlockInsertionPolicy::NewParagraph;
    std::vector<OUString> maCompatibleKits; // kit ids this block is designed for
};

/** Static, data-driven catalog of Editorial Blocks. */
class SVXCORE_DLLPUBLIC Writer2027EditorialBlockCatalog
{
public:
    static const Writer2027EditorialBlockCatalog& Get();

    const std::vector<EditorialBlockDefinition>& GetBlocks() const { return maBlocks; }

    /** Exact id match. Null when unknown. */
    const EditorialBlockDefinition* FindBlock(const OUString& rId) const;

private:
    Writer2027EditorialBlockCatalog();

    std::vector<EditorialBlockDefinition> maBlocks;
};

/** One curated Document Kit. */
struct DocumentKit
{
    OUString maId;              // stable id, e.g. "modern-product"
    TranslateId maNameResId;    // localized name
    TranslateId maDescriptionResId; // localized description
    TranslateId maUseResId;     // localized "recommended use" line
    OUString maTypeSystemId;    // recommended Type System preset id (never forced)
    std::vector<OUString> maRecommendedBlocks; // block ids recommended by this kit
};

/** Static, data-driven catalog of Document Kits. */
class SVXCORE_DLLPUBLIC Writer2027DocumentKitCatalog
{
public:
    static const Writer2027DocumentKitCatalog& Get();

    const std::vector<DocumentKit>& GetKits() const { return maKits; }

    /** Exact id match. Null when unknown. */
    const DocumentKit* FindKit(const OUString& rId) const;

private:
    Writer2027DocumentKitCatalog();

    std::vector<DocumentKit> maKits;
};

} // namespace svx::writer2027

#endif // INCLUDED_SVX_WRITER2027BLOCKS_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */