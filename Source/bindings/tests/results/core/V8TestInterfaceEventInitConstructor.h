// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py. DO NOT MODIFY!

#ifndef V8TestInterfaceEventInitConstructor_h
#define V8TestInterfaceEventInitConstructor_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8Event.h"
#include "bindings/core/v8/WrapperTypeInfo.h"
#include "bindings/tests/idls/core/TestInterfaceEventInitConstructor.h"
#include "platform/heap/Handle.h"

namespace blink {

class V8TestInterfaceEventInitConstructor {
public:
    static bool hasInstance(v8::Local<v8::Value>, v8::Isolate*);
    static v8::Local<v8::Object> findInstanceInPrototypeChain(v8::Local<v8::Value>, v8::Isolate*);
    static v8::Local<v8::FunctionTemplate> domTemplate(v8::Isolate*);
    static TestInterfaceEventInitConstructor* toImpl(v8::Local<v8::Object> object)
    {
        return blink::toScriptWrappable(object)->toImpl<TestInterfaceEventInitConstructor>();
    }
    static TestInterfaceEventInitConstructor* toImplWithTypeCheck(v8::Isolate*, v8::Local<v8::Value>);
    static const WrapperTypeInfo wrapperTypeInfo;
    static void refObject(ScriptWrappable*);
    static void derefObject(ScriptWrappable*);
    static void trace(Visitor* visitor, ScriptWrappable* scriptWrappable)
    {
#if ENABLE(OILPAN)
        visitor->trace(scriptWrappable->toImpl<TestInterfaceEventInitConstructor>());
#endif
    }
    static void constructorCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static const int internalFieldCount = v8DefaultWrapperInternalFieldCount + 0;
    static void installConditionallyEnabledProperties(v8::Local<v8::Object>, v8::Isolate*) { }
    static void installConditionallyEnabledMethods(v8::Local<v8::Object>, v8::Isolate*) { }
};

} // namespace blink

#endif // V8TestInterfaceEventInitConstructor_h
