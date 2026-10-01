/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#ifndef INCLUDED_SW_INC_WRITER2027_HXX
#define INCLUDED_SW_INC_WRITER2027_HXX

#include <tools/color.hxx>

// Writer 2027 — digital-document palette (Phase 1).
//
// Single source of truth for the colors applied to brand-new normal Writer
// documents (see ApplyDigitalDocumentDefaults in
// sw/source/uibase/app/docshini.cxx). These are *document* colors, not UI
// theme colors: the page is a real dark page and the text is real light
// text, so PDF export preserves the authored appearance.
//
// Later UI/theme work may replace these constants; keep the values here so
// every consumer changes in lockstep.
namespace sw::writer2027
{
/// Digital page background (near-black, premium, not pure #000).
constexpr Color PageBackground(0x12, 0x17, 0x1D);
/// Primary document text on PageBackground (WCAG AAA).
constexpr Color TextPrimary(0xE7, 0xED, 0xF3);
/// Secondary document text (reserved; captions, notes, muted content).
constexpr Color TextSecondary(0xAA, 0xB5, 0xC0);
/// Hairline for document structure (table borders, footnote separator).
constexpr Color Hairline(0x31, 0x40, 0x4A);
/// Unvisited hyperlink on PageBackground (WCAG AAA).
constexpr Color Link(0x4F, 0xC3, 0xF7);
/// Visited hyperlink on PageBackground (WCAG AAA).
constexpr Color LinkVisited(0xB3, 0x9D, 0xDB);
}

#endif // INCLUDED_SW_INC_WRITER2027_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */