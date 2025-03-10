// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/V8Binding.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ToV8.h"
#include "wtf/Vector.h"
#include <gtest/gtest.h>

namespace blink {

namespace {

class V8BindingTest : public ::testing::Test {
public:
    V8BindingTest() : m_scope(v8::Isolate::GetCurrent()) { }

    template<typename T> v8::Local<v8::Value> toV8(T value)
    {
        return blink::toV8(value, m_scope.scriptState()->context()->Global(), m_scope.isolate());
    }

    V8TestingScope m_scope;
};

TEST_F(V8BindingTest, toImplArray)
{
    {
        v8::Handle<v8::Array> v8StringArray = v8::Array::New(m_scope.isolate(), 2);
        v8StringArray->Set(toV8(0), toV8("Hello, World!"));
        v8StringArray->Set(toV8(1), toV8("Hi, Mom!"));

        NonThrowableExceptionState exceptionState;
        Vector<String> stringVector = toImplArray<String>(v8StringArray, 0, m_scope.isolate(), exceptionState);
        EXPECT_EQ(2U, stringVector.size());
        EXPECT_EQ("Hello, World!", stringVector[0]);
        EXPECT_EQ("Hi, Mom!", stringVector[1]);
    }
    {
        v8::Handle<v8::Array> v8UnsignedArray = v8::Array::New(m_scope.isolate(), 3);
        v8UnsignedArray->Set(toV8(0), toV8(42));
        v8UnsignedArray->Set(toV8(1), toV8(1729));
        v8UnsignedArray->Set(toV8(2), toV8(31773));

        NonThrowableExceptionState exceptionState;
        Vector<unsigned> unsignedVector = toImplArray<unsigned>(v8UnsignedArray, 0, m_scope.isolate(), exceptionState);
        EXPECT_EQ(3U, unsignedVector.size());
        EXPECT_EQ(42U, unsignedVector[0]);
        EXPECT_EQ(1729U, unsignedVector[1]);
        EXPECT_EQ(31773U, unsignedVector[2]);
    }
    {
        const double doublePi = 3.141592653589793238;
        const float floatPi = doublePi;
        v8::Handle<v8::Array> v8RealArray = v8::Array::New(m_scope.isolate(), 1);
        v8RealArray->Set(toV8(0), toV8(doublePi));

        NonThrowableExceptionState exceptionState;
        Vector<double> doubleVector = toImplArray<double>(v8RealArray, 0, m_scope.isolate(), exceptionState);
        EXPECT_EQ(1U, doubleVector.size());
        EXPECT_EQ(doublePi, doubleVector[0]);

        Vector<float> floatVector = toImplArray<float>(v8RealArray, 0, m_scope.isolate(), exceptionState);
        EXPECT_EQ(1U, floatVector.size());
        EXPECT_EQ(floatPi, floatVector[0]);
    }
    {
        v8::Handle<v8::Array> v8Array = v8::Array::New(m_scope.isolate(), 3);
        v8Array->Set(toV8(0), toV8("Vini, vidi, vici."));
        v8Array->Set(toV8(1), toV8(65535));
        v8Array->Set(toV8(2), toV8(0.125));

        NonThrowableExceptionState exceptionState;
        Vector<v8::Local<v8::Value> > v8HandleVector = toImplArray<v8::Local<v8::Value> >(v8Array, 0, m_scope.isolate(), exceptionState);
        EXPECT_EQ(3U, v8HandleVector.size());
        EXPECT_EQ("Vini, vidi, vici.", toUSVString(v8HandleVector[0], exceptionState));
        EXPECT_EQ(65535U, toUInt32(v8HandleVector[1]));
        EXPECT_EQ(0.125, toFloat(v8HandleVector[2]));

        Vector<ScriptValue> scriptValueVector = toImplArray<ScriptValue>(v8Array, 0, m_scope.isolate(), exceptionState);
        EXPECT_EQ(3U, scriptValueVector.size());
        String reportOnZela;
        EXPECT_TRUE(scriptValueVector[0].toString(reportOnZela));
        EXPECT_EQ("Vini, vidi, vici.", reportOnZela);
        EXPECT_EQ(65535U, toUInt32(scriptValueVector[1].v8Value()));
        EXPECT_EQ(0.125, toFloat(scriptValueVector[2].v8Value()));
    }
    {
        v8::Handle<v8::Array> v8StringArray1 = v8::Array::New(m_scope.isolate(), 2);
        v8StringArray1->Set(toV8(0), toV8("foo"));
        v8StringArray1->Set(toV8(1), toV8("bar"));
        v8::Handle<v8::Array> v8StringArray2 = v8::Array::New(m_scope.isolate(), 3);
        v8StringArray2->Set(toV8(0), toV8("x"));
        v8StringArray2->Set(toV8(1), toV8("y"));
        v8StringArray2->Set(toV8(2), toV8("z"));
        v8::Handle<v8::Array> v8StringArrayArray = v8::Array::New(m_scope.isolate(), 2);
        v8StringArrayArray->Set(toV8(0), v8StringArray1);
        v8StringArrayArray->Set(toV8(1), v8StringArray2);

        NonThrowableExceptionState exceptionState;
        Vector<Vector<String> > stringVectorVector = toImplArray<Vector<String> >(v8StringArrayArray, 0, m_scope.isolate(), exceptionState);
        EXPECT_EQ(2U, stringVectorVector.size());
        EXPECT_EQ(2U, stringVectorVector[0].size());
        EXPECT_EQ("foo", stringVectorVector[0][0]);
        EXPECT_EQ("bar", stringVectorVector[0][1]);
        EXPECT_EQ(3U, stringVectorVector[1].size());
        EXPECT_EQ("x", stringVectorVector[1][0]);
        EXPECT_EQ("y", stringVectorVector[1][1]);
        EXPECT_EQ("z", stringVectorVector[1][2]);
    }
}

} // namespace

} // namespace blink
