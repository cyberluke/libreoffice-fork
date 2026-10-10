/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SVX_WRITER2027CONTRAST_HXX
#define INCLUDED_SVX_WRITER2027CONTRAST_HXX

#include <rtl/ustring.hxx>
#include <sal/types.h>

#include <svx/svxdllapi.h>

#include <tools/color.hxx>

#include <algorithm>
#include <cmath>

namespace svx::writer2027
{

/** WCAG relative luminance of one 8-bit sRGB channel (0..1).
    https://www.w3.org/WAI/GL/wiki/Relative_luminance */
inline double lcl_Writer2027ChannelLuminance(sal_uInt8 nByte)
{
    const double fLinear = nByte / 255.0;
    return (fLinear <= 0.04045)
               ? (fLinear / 12.92)
               : std::pow((fLinear + 0.055) / 1.055, 2.4);
}

/** WCAG relative luminance of a color (0..1), using linearized sRGB channels. */
inline double Writer2027RelativeLuminance(const Color& rColor)
{
    const double fR = lcl_Writer2027ChannelLuminance(rColor.GetRed());
    const double fG = lcl_Writer2027ChannelLuminance(rColor.GetGreen());
    const double fB = lcl_Writer2027ChannelLuminance(rColor.GetBlue());
    return 0.2126 * fR + 0.7152 * fG + 0.0722 * fB;
}

/** WCAG contrast ratio between two colors (>= 1.0). */
inline double Writer2027ContrastRatio(const Color& rA, const Color& rB)
{
    const double fL1 = Writer2027RelativeLuminance(rA);
    const double fL2 = Writer2027RelativeLuminance(rB);
    const double fHi = std::max(fL1, fL2);
    const double fLo = std::min(fL1, fL2);
    return (fHi + 0.05) / (fLo + 0.05);
}

/** Choose a foreground that meets a minimum contrast ratio against a given
    background.

    Returns rPreferred when it already satisfies rMinRatio; otherwise the first
    of the fallback candidates (in order) that does; otherwise the candidate
    with the highest computed contrast. This uses real WCAG relative luminance,
    not a naive 0..255 channel subtraction, so it stays correct on both light
    and dark surfaces (spec 6 / 49.3 / 70). */
inline Color Writer2027EnsureTextContrast(const Color& rPreferred, const Color& rBackground,
                                          const Color& rFallback, double rMinRatio)
{
    if (Writer2027ContrastRatio(rPreferred, rBackground) >= rMinRatio)
        return rPreferred;
    if (Writer2027ContrastRatio(rFallback, rBackground) >= rMinRatio)
        return rFallback;
    // Neither meets the bar: pick whichever is least wrong.
    return (Writer2027ContrastRatio(rPreferred, rBackground)
            >= Writer2027ContrastRatio(rFallback, rBackground))
               ? rPreferred
               : rFallback;
}

} // namespace svx::writer2027

#endif // INCLUDED_SVX_WRITER2027CONTRAST_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */