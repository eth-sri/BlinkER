/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/svg/SVGStopElement.h"

#include "core/rendering/svg/RenderSVGGradientStop.h"

namespace blink {

inline SVGStopElement::SVGStopElement(Document& document)
    : SVGElement(SVGNames::stopTag, document)
    , m_offset(SVGAnimatedNumber::create(this, SVGNames::offsetAttr, SVGNumberAcceptPercentage::create()))
{
    addToPropertyMap(m_offset);
}

void SVGStopElement::trace(Visitor* visitor)
{
    visitor->trace(m_offset);
    SVGElement::trace(visitor);
}

DEFINE_NODE_FACTORY(SVGStopElement)

void SVGStopElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    parseAttributeNew(name, value);
}

void SVGStopElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::offsetAttr) {
        SVGElement::InvalidationGuard invalidationGuard(this);

        if (renderer())
            markForLayoutAndParentResourceInvalidation(renderer());
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

RenderObject* SVGStopElement::createRenderer(RenderStyle*)
{
    return new RenderSVGGradientStop(this);
}

bool SVGStopElement::rendererIsNeeded(const RenderStyle&)
{
    return true;
}

Color SVGStopElement::stopColorIncludingOpacity() const
{
    RenderStyle* style = renderer() ? renderer()->style() : 0;
    // FIXME: This check for null style exists to address Bug WK 90814, a rare crash condition in
    // which the renderer or style is null. This entire class is scheduled for removal (Bug WK 86941)
    // and we will tolerate this null check until then.
    if (!style)
        return Color(Color::transparent); // Transparent black.

    const SVGRenderStyle& svgStyle = style->svgStyle();
    return svgStyle.stopColor().combineWithAlpha(svgStyle.stopOpacity());
}

} // namespace blink
