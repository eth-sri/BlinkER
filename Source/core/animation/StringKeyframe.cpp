// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/StringKeyframe.h"

#include "core/animation/ColorStyleInterpolation.h"
#include "core/animation/DefaultStyleInterpolation.h"
#include "core/animation/DeferredLegacyStyleInterpolation.h"
#include "core/animation/DoubleStyleInterpolation.h"
#include "core/animation/ImageStyleInterpolation.h"
#include "core/animation/LegacyStyleInterpolation.h"
#include "core/animation/LengthBoxStyleInterpolation.h"
#include "core/animation/LengthPairStyleInterpolation.h"
#include "core/animation/LengthPoint3DStyleInterpolation.h"
#include "core/animation/LengthStyleInterpolation.h"
#include "core/animation/VisibilityStyleInterpolation.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/rendering/style/RenderStyle.h"

namespace blink {

StringKeyframe::StringKeyframe(const StringKeyframe& copyFrom)
    : Keyframe(copyFrom.m_offset, copyFrom.m_composite, copyFrom.m_easing)
    , m_propertySet(copyFrom.m_propertySet->mutableCopy())
{
}

void StringKeyframe::setPropertyValue(CSSPropertyID property, const String& value, StyleSheetContents* styleSheetContents)
{
    ASSERT(property != CSSPropertyInvalid);
    if (CSSAnimations::isAllowedAnimation(property))
        m_propertySet->setProperty(property, value, false, styleSheetContents);
}

PropertySet StringKeyframe::properties() const
{
    // This is not used in time-critical code, so we probably don't need to
    // worry about caching this result.
    PropertySet properties;
    for (unsigned i = 0; i < m_propertySet->propertyCount(); ++i)
        properties.add(m_propertySet->propertyAt(i).id());
    return properties;
}

PassRefPtrWillBeRawPtr<Keyframe> StringKeyframe::clone() const
{
    return adoptRefWillBeNoop(new StringKeyframe(*this));
}
PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> StringKeyframe::createPropertySpecificKeyframe(CSSPropertyID property) const
{
    return adoptPtrWillBeNoop(new PropertySpecificKeyframe(offset(), &easing(), propertyValue(property), composite()));
}

void StringKeyframe::trace(Visitor* visitor)
{
    visitor->trace(m_propertySet);
    Keyframe::trace(visitor);
}

StringKeyframe::PropertySpecificKeyframe::PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, CSSValue* value, AnimationEffect::CompositeOperation op)
    : Keyframe::PropertySpecificKeyframe(offset, easing, op)
    , m_value(value)
{ }

StringKeyframe::PropertySpecificKeyframe::PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, CSSValue* value)
    : Keyframe::PropertySpecificKeyframe(offset, easing, AnimationEffect::CompositeReplace)
    , m_value(value)
{
    ASSERT(!isNull(m_offset));
}

PassRefPtrWillBeRawPtr<Interpolation> StringKeyframe::PropertySpecificKeyframe::createInterpolation(CSSPropertyID property, Keyframe::PropertySpecificKeyframe* end, Element* element) const
{
    CSSValue* fromCSSValue = m_value.get();
    CSSValue* toCSSValue = toStringPropertySpecificKeyframe(end)->value();
    ValueRange range = ValueRangeAll;

    if (!CSSPropertyMetadata::isAnimatableProperty(property))
        return DefaultStyleInterpolation::create(fromCSSValue, toCSSValue, property);

    bool useDefaultStyleInterpolation = true;

    switch (property) {
    case CSSPropertyLineHeight:
        // FIXME: Handle numbers.
        useDefaultStyleInterpolation = false;
        // Fall through
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyFontSize:
    case CSSPropertyHeight:
    case CSSPropertyMaxHeight:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyMotionPosition:
    case CSSPropertyOutlineWidth:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyPerspective:
    case CSSPropertyShapeMargin:
    case CSSPropertyWidth:
        range = ValueRangeNonNegative;
        // Fall through
    case CSSPropertyBottom:
    case CSSPropertyLeft:
    case CSSPropertyLetterSpacing:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyOutlineOffset:
    case CSSPropertyRight:
    case CSSPropertyTop:
    case CSSPropertyVerticalAlign:
    case CSSPropertyWordSpacing:
        if (LengthStyleInterpolation::canCreateFrom(*fromCSSValue) && LengthStyleInterpolation::canCreateFrom(*toCSSValue))
            return LengthStyleInterpolation::create(*fromCSSValue, *toCSSValue, property, range);
        // FIXME: Handle keywords e.g. 'none'.
        if (property == CSSPropertyPerspective)
            useDefaultStyleInterpolation = false;
        break;
    case CSSPropertyMotionRotation: {
        RefPtrWillBeRawPtr<Interpolation> interpolation = DoubleStyleInterpolation::maybeCreateFromMotionRotation(*fromCSSValue, *toCSSValue, property);
        if (interpolation)
            return interpolation.release();
        break;
    }
    case CSSPropertyVisibility:
        if (VisibilityStyleInterpolation::canCreateFrom(*fromCSSValue) && VisibilityStyleInterpolation::canCreateFrom(*toCSSValue) && (VisibilityStyleInterpolation::isVisible(*fromCSSValue) || VisibilityStyleInterpolation::isVisible(*toCSSValue))) {
            return VisibilityStyleInterpolation::create(*fromCSSValue, *toCSSValue, property);
        }
        break;
    case CSSPropertyFill:
    case CSSPropertyStroke:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyColor:
    case CSSPropertyFloodColor:
    case CSSPropertyLightingColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyStopColor:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitTextStrokeColor:
        if (ColorStyleInterpolation::canCreateFrom(*fromCSSValue) && ColorStyleInterpolation::canCreateFrom(*toCSSValue))
            return ColorStyleInterpolation::create(*fromCSSValue, *toCSSValue, property);
        // FIXME: Handle color keyword cases e.g. black.
        useDefaultStyleInterpolation = false;
        break;

    case CSSPropertyBorderImageSource:
    case CSSPropertyListStyleImage:
    case CSSPropertyWebkitMaskBoxImageSource:
        if (ImageStyleInterpolation::canCreateFrom(*fromCSSValue) && ImageStyleInterpolation::canCreateFrom(*toCSSValue))
            return ImageStyleInterpolation::create(*fromCSSValue, *toCSSValue, property);
        break;

    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
        range = ValueRangeNonNegative;
        // Fall through
    case CSSPropertyPerspectiveOrigin:
    case CSSPropertyObjectPosition:
        if (LengthPairStyleInterpolation::canCreateFrom(*fromCSSValue) && LengthPairStyleInterpolation::canCreateFrom(*toCSSValue))
            return LengthPairStyleInterpolation::create(*fromCSSValue, *toCSSValue, property, range);
        // FIXME: Handle CSSValueLists.
        useDefaultStyleInterpolation = false;
        break;

    case CSSPropertyTransformOrigin:
        if (LengthPoint3DStyleInterpolation::canCreateFrom(*fromCSSValue) && LengthPoint3DStyleInterpolation::canCreateFrom(*toCSSValue))
            return LengthPoint3DStyleInterpolation::create(*fromCSSValue, *toCSSValue, property);
        // FIXME: Handle 2D origins.
        useDefaultStyleInterpolation = false;
        break;

    case CSSPropertyWebkitMaskBoxImageSlice:
        if (LengthBoxStyleInterpolation::matchingFill(*toCSSValue, *fromCSSValue) && LengthBoxStyleInterpolation::canCreateFrom(*fromCSSValue) && LengthStyleInterpolation::canCreateFrom(*toCSSValue))
            return LengthBoxStyleInterpolation::createFromBorderImageSlice(*fromCSSValue, *toCSSValue, property);
        break;

    default:
        useDefaultStyleInterpolation = false;
        break;
    }

    if (DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(*fromCSSValue) || DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(*toCSSValue))
        return DeferredLegacyStyleInterpolation::create(fromCSSValue, toCSSValue, property);

    if (useDefaultStyleInterpolation) {
        ASSERT(AnimatableValue::usesDefaultInterpolation(
            StyleResolver::createAnimatableValueSnapshot(*element, property, *fromCSSValue).get(),
            StyleResolver::createAnimatableValueSnapshot(*element, property, *toCSSValue).get()));
        return DefaultStyleInterpolation::create(fromCSSValue, toCSSValue, property);
    }

    // FIXME: Remove the use of AnimatableValues, RenderStyles and Elements here.
    // FIXME: Remove this cache
    ASSERT(element);
    if (!m_animatableValueCache)
        m_animatableValueCache = StyleResolver::createAnimatableValueSnapshot(*element, property, *fromCSSValue);

    RefPtrWillBeRawPtr<AnimatableValue> to = StyleResolver::createAnimatableValueSnapshot(*element, property, *toCSSValue);
    toStringPropertySpecificKeyframe(end)->m_animatableValueCache = to;

    return LegacyStyleInterpolation::create(m_animatableValueCache.get(), to.release(), property);
}

PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> StringKeyframe::PropertySpecificKeyframe::neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const
{
    return adoptPtrWillBeNoop(new PropertySpecificKeyframe(offset, easing, 0, AnimationEffect::CompositeAdd));
}

PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> StringKeyframe::PropertySpecificKeyframe::cloneWithOffset(double offset) const
{
    Keyframe::PropertySpecificKeyframe* theClone = new PropertySpecificKeyframe(offset, m_easing, m_value.get());
    toStringPropertySpecificKeyframe(theClone)->m_animatableValueCache = m_animatableValueCache;
    return adoptPtrWillBeNoop(theClone);
}

void StringKeyframe::PropertySpecificKeyframe::trace(Visitor* visitor)
{
    visitor->trace(m_value);
    visitor->trace(m_animatableValueCache);
    Keyframe::PropertySpecificKeyframe::trace(visitor);
}

}
