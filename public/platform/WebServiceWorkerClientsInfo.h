// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerClientsInfo_h
#define WebServiceWorkerClientsInfo_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebPageVisibilityState.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebVector.h"

namespace blink {

struct WebServiceWorkerError;

struct WebServiceWorkerClientInfo {
    WebServiceWorkerClientInfo()
        : clientID(0)
        , pageVisibilityState(WebPageVisibilityStateLast)
        , isFocused(false)
        , frameType(WebURLRequest::FrameTypeNone)
    {
    }

    int clientID;

    WebPageVisibilityState pageVisibilityState;
    bool isFocused;
    WebURL url;
    WebURLRequest::FrameType frameType;
};

struct WebServiceWorkerClientsInfo {
    WebVector<WebServiceWorkerClientInfo> clients;
};

typedef WebCallbacks<WebServiceWorkerClientsInfo, WebServiceWorkerError> WebServiceWorkerClientsCallbacks;

} // namespace blink

#endif // WebServiceWorkerClientsInfo_h
