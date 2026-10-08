/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SW_WRITER2027BLOCKS_HXX
#define INCLUDED_SW_WRITER2027BLOCKS_HXX

#include <rtl/ustring.hxx>

class FontList;
class SwWrtShell;

namespace svx::writer2027
{
struct EditorialBlockDefinition;
}

namespace sw::writer2027blocks
{

/** Insert one editorial block into the document through the current shell.

    Builds the block exclusively from canonical Writer primitives (paragraphs,
    paragraph styles, sections with columns, anchored frames) using the block's
    own minimal role-based styles which derive from the canonical pool styles
    (so the block inherits the active Phase 6 Type System; Custom typography is
    the actual current pool styles - no hardcoded font families).

    The whole insertion - including any lazily created block styles - is one
    grouped undo action (SwUndoId::WRITER2027_BLOCK_INSERT), so one block
    insertion is one logical Undo/Redo step. After insertion the caret is
    placed into the block's most useful editable field with the placeholder
    text selected, so the user can type immediately (placeholder is replaced).

    pFontList is only used for family metric lookups of pool styles and may be
    null. Returns false when the block could not be inserted (e.g. read-only
    shell) - the caller should then not report success.
 */
bool InsertEditorialBlock(SwWrtShell& rSh,
                          const svx::writer2027::EditorialBlockDefinition& rBlock,
                          const FontList* pFontList);

} // namespace sw::writer2027blocks

#endif // INCLUDED_SW_WRITER2027BLOCKS_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */