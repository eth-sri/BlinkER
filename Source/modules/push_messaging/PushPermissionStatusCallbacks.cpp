// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushPermissionStatusCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/ExceptionCode.h"
#include "wtf/text/WTFString.h"

namespace blink {

PushPermissionStatusCallbacks::PushPermissionStatusCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
    : m_resolver(resolver)
{
}

PushPermissionStatusCallbacks::~PushPermissionStatusCallbacks()
{
}

void PushPermissionStatusCallbacks::onSuccess(WebPushPermissionStatus* status)
{
    m_resolver->resolve(permissionString(*status));
}

void PushPermissionStatusCallbacks::onError()
{
    m_resolver->reject();
}

// static
const String& PushPermissionStatusCallbacks::permissionString(WebPushPermissionStatus status)
{
    DEFINE_STATIC_LOCAL(const String, grantedPermission, ("granted"));
    DEFINE_STATIC_LOCAL(const String, deniedPermission, ("denied"));
    DEFINE_STATIC_LOCAL(const String, defaultPermission, ("default"));

    switch (status) {
    case WebPushPermissionStatusGranted:
        return grantedPermission;
    case WebPushPermissionStatusDenied:
        return deniedPermission;
    case WebPushPermissionStatusDefault:
        return defaultPermission;
    }

    ASSERT_NOT_REACHED();
    return deniedPermission;
}

} // namespace blink
