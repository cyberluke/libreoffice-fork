/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SW_WRITER2027TYPOGRAPHYMANAGER_HXX
#define INCLUDED_SW_WRITER2027TYPOGRAPHYMANAGER_HXX

#include <rtl/ustring.hxx>
#include <com/sun/star/uno/Reference.hxx>

#include <swdllapi.h>

#include <memory>
#include <vector>

namespace com::sun::star::frame { class XFrame; }

class FontList;
class SwDoc;
class SwDocShell;

namespace svx::writer2027
{
struct TypeSystemPreset;
struct ResolvedTypeSystem;
enum class TypeSystemFontRole;
}

namespace sw::writer2027typographymanager
{

/** Which field of the TypeSystemScale a semantic style maps to. */
enum class TypeSystemScaleSlot
{
    None,
    Body,
    Heading1,
    Heading2,
    Heading3,
    Heading4,
    Heading5,
    Heading6,
    Title,
    Subtitle,
    Quote,
    Caption,
    Mono
};

/** Canonical Writer 2027 semantic style roles (spec 19). */
enum class Writer2027SemanticStyle
{
    DefaultBody,
    Body,
    Heading1,
    Heading2,
    Heading3,
    Heading4,
    Heading5,
    Heading6,
    Title,
    Subtitle,
    Quote,
    Caption,
    CodeBlock,
    InlineCode
};

/** One row of the canonical semantic style map: role -> pool id + font role +
    scale slot + UI label. This single table is the source of truth for Type
    System apply, detection, the Styles gallery, the preview labels and tests
    (spec 19). Never duplicate the mapping in scattered switch statements. */
struct SW_DLLPUBLIC Writer2027SemanticStyleDescriptor
{
    Writer2027SemanticStyle meRole;
    int mnPoolId; // SwPoolFormatId value (int to avoid the enum typedef)
    bool mbCharacterStyle;
    svx::writer2027::TypeSystemFontRole meFontRole;
    TypeSystemScaleSlot meScaleSlot;
    OUString maUiLabel;
};

/** The shared semantic style table. */
SW_DLLPUBLIC const std::vector<Writer2027SemanticStyleDescriptor>&
GetWriter2027SemanticStyleDescriptors();

/** Look up a descriptor by role. Null when unknown. */
SW_DLLPUBLIC const Writer2027SemanticStyleDescriptor*
GetWriter2027SemanticStyleDescriptor(Writer2027SemanticStyle eRole);

/** Document-bound typography context for Writer 2027 UI.

    Resolved from an owning frame, never from SfxObjectShell::Current() or
    SwModule::GetFirstView() (spec 28/77). All Writer 2027 document-bound
    operations (preview resolution, detection, apply, style gallery) use the
    same context so there is exactly one real FontList and one real SwDoc.
 */
struct Writer2027DocumentTypographyContext
{
    SwDocShell* pDocShell = nullptr;
    SwDoc* pDoc = nullptr;
    const FontList* pFontList = nullptr;
    css::uno::Reference<css::frame::XFrame> xFrame;
};

/** Resolve the typography context for the given frame. Returns a context with
    null members when the frame has no Writer controller/model/docshell. */
SW_DLLPUBLIC Writer2027DocumentTypographyContext
ResolveWriter2027TypographyContext(const css::uno::Reference<css::frame::XFrame>& rFrame);

/** Materialize the Writer 2027 semantic built-in styles in rDoc so they exist
    in the pool and can be previewed/applied/updated (spec 20). Returns true on
    success. */
SW_DLLPUBLIC bool EnsureSemanticStylesMaterialized(SwDoc& rDoc);

} // namespace sw::writer2027typographymanager

#endif // INCLUDED_SW_WRITER2027TYPOGRAPHYMANAGER_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */