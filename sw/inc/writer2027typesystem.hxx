/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SW_WRITER2027TYPESYSTEM_HXX
#define INCLUDED_SW_WRITER2027TYPESYSTEM_HXX

#include <rtl/ustring.hxx>

class FontList;
class SwDoc;

namespace svx::writer2027
{
struct TypeSystemPreset;
struct ResolvedTypeSystem;
}

namespace sw::writer2027typesystem
{

/** Apply a resolved Type System to the canonical Writer styles of rDoc.

    Mutates pool styles only (Default Paragraph Style, Heading, Heading 1-3,
    Title, Subtitle, Block Quotation, Caption, Preformatted Text, Source
    Text). Never touches paragraph/character content. The whole preset is
    one grouped undo action (SwUndoId::WRITER2027_TYPE_SYSTEM) and the
    document is marked modified normally. pFontList is used to build the
    font items from real installed metrics and may be null (the resolved
    family names are then stored as-is).
 */
bool ApplyTypeSystem(SwDoc& rDoc, const svx::writer2027::TypeSystemPreset& rPreset,
                     const svx::writer2027::ResolvedTypeSystem& rResolved,
                     const FontList* pFontList);

/** Current-preset detection.

    Compares the bounded set of preset-owned style attributes (body font
    family/size/spacing, heading family and H1-H3/Title sizes, mono family)
    against each catalog preset resolved for the current font installation.
    Returns the id of the first matching preset, or an empty string when the
    document's styles no longer match any preset ("Custom").

    Only invoked on popup open / after applying a preset - never during
    painting.
 */
OUString DetectCurrentTypeSystem(SwDoc& rDoc, const FontList* pFontList);

} // namespace sw::writer2027typesystem

#endif // INCLUDED_SW_WRITER2027TYPESYSTEM_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */