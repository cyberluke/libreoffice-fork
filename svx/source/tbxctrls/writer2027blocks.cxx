/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <svx/writer2027blocks.hxx>

#include <svx/strings.hrc>

#include <utility>

namespace svx::writer2027
{

namespace
{

// Curated block table. Pure data: the insertion service interprets each
// block id and builds the block from canonical Writer primitives. The
// category decides the gallery grouping; the policy decides the flow entry.
struct BlockSeed
{
    const char* pId;
    TranslateId aNameResId;
    TranslateId aDescriptionResId;
    EditorialBlockCategory eCategory;
    EditorialBlockInsertionPolicy ePolicy;
    const char* const* pKits; // null-terminated kit id list
};

const char* const aKitsModernProduct[] = { "modern-product", nullptr };
const char* const aKitsEditorial[] = { "editorial", nullptr };
const char* const aKitsExecutive[] = { "executive", nullptr };
const char* const aKitsResearch[] = { "research", nullptr };
const char* const aKitsCreativeAgency[] = { "creative-agency", nullptr };
const char* const aKitsEditorialCreative[] = { "editorial", "creative-agency", nullptr };
const char* const aKitsAll[] = { "modern-product", "editorial", "executive", "research",
                                 "creative-agency", nullptr };

const BlockSeed aBlockSeeds[] = {
    // Hero: cover opening. New page; reuses an empty document instead.
    { "hero", STR_WRITER2027_BLOCK_HERO, STR_WRITER2027_BLOCK_HERO_DESC,
      EditorialBlockCategory::Recommended, EditorialBlockInsertionPolicy::NewPage, aKitsAll },
    // Section Intro: number/eyebrow + heading + intro.
    { "section-intro", STR_WRITER2027_BLOCK_SECTION_INTRO, STR_WRITER2027_BLOCK_SECTION_INTRO_DESC,
      EditorialBlockCategory::Recommended, EditorialBlockInsertionPolicy::NewParagraph,
      aKitsAll },
    // Pull Quote: oversized quote + attribution.
    { "pull-quote", STR_WRITER2027_BLOCK_PULL_QUOTE, STR_WRITER2027_BLOCK_PULL_QUOTE_DESC,
      EditorialBlockCategory::Recommended, EditorialBlockInsertionPolicy::NewParagraph,
      aKitsEditorialCreative },
    // Key Stat: large number + label + optional supporting line.
    { "key-stat", STR_WRITER2027_BLOCK_KEY_STAT, STR_WRITER2027_BLOCK_KEY_STAT_DESC,
      EditorialBlockCategory::Recommended, EditorialBlockInsertionPolicy::NewParagraph,
      aKitsAll },
    // Image + Caption: frame placeholder + caption + source/credit.
    { "image-caption", STR_WRITER2027_BLOCK_IMAGE_CAPTION, STR_WRITER2027_BLOCK_IMAGE_CAPTION_DESC,
      EditorialBlockCategory::Editorial, EditorialBlockInsertionPolicy::NewParagraph,
      aKitsEditorialCreative },
    // Two Column: bounded section with two columns.
    { "two-column", STR_WRITER2027_BLOCK_TWO_COLUMN, STR_WRITER2027_BLOCK_TWO_COLUMN_DESC,
      EditorialBlockCategory::Editorial, EditorialBlockInsertionPolicy::NewSection, aKitsAll },
    // Callout: short heading + accented body.
    { "callout", STR_WRITER2027_BLOCK_CALLOUT, STR_WRITER2027_BLOCK_CALLOUT_DESC,
      EditorialBlockCategory::Editorial, EditorialBlockInsertionPolicy::NewParagraph, aKitsAll },
    // Research Note: label + title + body + source.
    { "research-note", STR_WRITER2027_BLOCK_RESEARCH_NOTE, STR_WRITER2027_BLOCK_RESEARCH_NOTE_DESC,
      EditorialBlockCategory::Technical, EditorialBlockInsertionPolicy::NewParagraph,
      aKitsResearch },
    // Code: monospace block via the Type System mono role.
    { "code", STR_WRITER2027_BLOCK_CODE, STR_WRITER2027_BLOCK_CODE_DESC,
      EditorialBlockCategory::Technical, EditorialBlockInsertionPolicy::NewParagraph,
      aKitsResearch },
    // Closing Statement: large final statement + small supporting line.
    { "closing-statement", STR_WRITER2027_BLOCK_CLOSING, STR_WRITER2027_BLOCK_CLOSING_DESC,
      EditorialBlockCategory::Recommended, EditorialBlockInsertionPolicy::NewPage, aKitsAll },
};

std::vector<OUString> lcl_Kits(const char* const* pKits)
{
    std::vector<OUString> aKits;
    for (; pKits && *pKits; ++pKits)
        aKits.emplace_back(OUString::fromUtf8(*pKits));
    return aKits;
}

} // namespace

Writer2027EditorialBlockCatalog::Writer2027EditorialBlockCatalog()
{
    for (const auto& rSeed : aBlockSeeds)
    {
        EditorialBlockDefinition aBlock;
        aBlock.maId = OUString::fromUtf8(rSeed.pId);
        aBlock.maNameResId = rSeed.aNameResId;
        aBlock.maDescriptionResId = rSeed.aDescriptionResId;
        aBlock.meCategory = rSeed.eCategory;
        aBlock.meInsertionPolicy = rSeed.ePolicy;
        aBlock.maCompatibleKits = lcl_Kits(rSeed.pKits);
        maBlocks.push_back(std::move(aBlock));
    }
}

const Writer2027EditorialBlockCatalog& Writer2027EditorialBlockCatalog::Get()
{
    static const Writer2027EditorialBlockCatalog aCatalog;
    return aCatalog;
}

const EditorialBlockDefinition* Writer2027EditorialBlockCatalog::FindBlock(const OUString& rId) const
{
    for (const auto& rBlock : maBlocks)
        if (rBlock.maId == rId)
            return &rBlock;
    return nullptr;
}

namespace
{

// Curated Document Kit table. Each kit references a preferred Type System
// preset id (Phase 6) and the block ids it recommends. The pairing is a
// recommendation, never a lock: the user may run the Editorial kit with
// Custom typography (§26).
struct KitSeed
{
    const char* pId;
    TranslateId aNameResId;
    TranslateId aDescriptionResId;
    TranslateId aUseResId;
    const char* pTypeSystemId;
    const char* const* pBlocks;
};

const char* const aModernProductBlocks[] = { "hero", "section-intro", "key-stat", "two-column",
                                             "callout", "closing-statement", nullptr };
const char* const aEditorialBlocks[] = { "hero", "section-intro", "pull-quote", "image-caption",
                                         "two-column", "closing-statement", nullptr };
const char* const aExecutiveBlocks[] = { "hero", "section-intro", "key-stat", "two-column",
                                         "callout", "closing-statement", nullptr };
const char* const aResearchBlocks[] = { "section-intro", "research-note", "code", "image-caption",
                                        "key-stat", "two-column", nullptr };
const char* const aCreativeAgencyBlocks[] = { "hero", "image-caption", "pull-quote", "section-intro",
                                              "closing-statement", nullptr };

const KitSeed aKitSeeds[] = {
    { "modern-product", STR_WRITER2027_KIT_MODERN_PRODUCT, STR_WRITER2027_KIT_MODERN_PRODUCT_DESC,
      STR_WRITER2027_KIT_MODERN_PRODUCT_USE, "modern-product", aModernProductBlocks },
    { "editorial", STR_WRITER2027_KIT_EDITORIAL, STR_WRITER2027_KIT_EDITORIAL_DESC,
      STR_WRITER2027_KIT_EDITORIAL_USE, "editorial", aEditorialBlocks },
    { "executive", STR_WRITER2027_KIT_EXECUTIVE, STR_WRITER2027_KIT_EXECUTIVE_DESC,
      STR_WRITER2027_KIT_EXECUTIVE_USE, "executive", aExecutiveBlocks },
    { "research", STR_WRITER2027_KIT_RESEARCH, STR_WRITER2027_KIT_RESEARCH_DESC,
      STR_WRITER2027_KIT_RESEARCH_USE, "research", aResearchBlocks },
    { "creative-agency", STR_WRITER2027_KIT_CREATIVE_AGENCY, STR_WRITER2027_KIT_CREATIVE_AGENCY_DESC,
      STR_WRITER2027_KIT_CREATIVE_AGENCY_USE, "creative-agency", aCreativeAgencyBlocks },
};

} // namespace

Writer2027DocumentKitCatalog::Writer2027DocumentKitCatalog()
{
    for (const auto& rSeed : aKitSeeds)
    {
        DocumentKit aKit;
        aKit.maId = OUString::fromUtf8(rSeed.pId);
        aKit.maNameResId = rSeed.aNameResId;
        aKit.maDescriptionResId = rSeed.aDescriptionResId;
        aKit.maUseResId = rSeed.aUseResId;
        if (rSeed.pTypeSystemId)
            aKit.maTypeSystemId = OUString::fromUtf8(rSeed.pTypeSystemId);
        aKit.maRecommendedBlocks = lcl_Kits(rSeed.pBlocks);
        maKits.push_back(std::move(aKit));
    }
}

const Writer2027DocumentKitCatalog& Writer2027DocumentKitCatalog::Get()
{
    static const Writer2027DocumentKitCatalog aCatalog;
    return aCatalog;
}

const DocumentKit* Writer2027DocumentKitCatalog::FindKit(const OUString& rId) const
{
    for (const auto& rKit : maKits)
        if (rKit.maId == rId)
            return &rKit;
    return nullptr;
}

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */