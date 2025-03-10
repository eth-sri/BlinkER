/*
 * Copyright (C) 2005, 2006, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceLoader_h
#define ResourceLoader_h

#include "core/fetch/ResourceLoaderOptions.h"
#include "platform/network/ResourceRequest.h"
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLLoaderClient.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"

namespace blink {
class WebThreadedDataReceiver;
}

namespace blink {

class EventAction;
class EventRacerLog;
class Resource;
class KURL;
class ResourceError;
class ResourceLoaderHost;

class ResourceLoader final : public RefCountedWillBeGarbageCollectedFinalized<ResourceLoader>, protected WebURLLoaderClient {
public:
    static PassRefPtrWillBeRawPtr<ResourceLoader> create(ResourceLoaderHost*, Resource*, const ResourceRequest&, const ResourceLoaderOptions&);
    virtual ~ResourceLoader();
    void trace(Visitor*);

    void start();
    void changeToSynchronous();

    void cancel();
    void cancel(const ResourceError&);
    void cancelIfNotFinishing();

    Resource* cachedResource() { return m_resource; }
    const ResourceRequest& originalRequest() const { return m_originalRequest; }

    void setDefersLoading(bool);
    bool defersLoading() const { return m_defersLoading; }

    void attachThreadedDataReceiver(PassOwnPtr<blink::WebThreadedDataReceiver>);

    void releaseResources();

    void didChangePriority(ResourceLoadPriority, int intraPriorityValue);

    // WebURLLoaderClient
    void willSendRequest(blink::WebURLLoader*, blink::WebURLRequest&, const blink::WebURLResponse& redirectResponse) override;
    void didSendData(blink::WebURLLoader*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override;
    void didReceiveResponse(blink::WebURLLoader*, const blink::WebURLResponse&) override;
    void didReceiveResponse(blink::WebURLLoader*, const blink::WebURLResponse&, WebDataConsumerHandle*) override;
    void didReceiveData(blink::WebURLLoader*, const char*, int, int encodedDataLength) override;
    void didReceiveCachedMetadata(blink::WebURLLoader*, const char* data, int length) override;
    void didFinishLoading(blink::WebURLLoader*, double finishTime, int64_t encodedDataLength) override;
    void didFail(blink::WebURLLoader*, const blink::WebURLError&) override;
    void didDownloadData(blink::WebURLLoader*, int, int) override;

    const KURL& url() const { return m_request.url(); }
    bool isLoadedBy(ResourceLoaderHost*) const;

    bool reachedTerminalState() const { return m_state == Terminated; }
    const ResourceRequest& request() const { return m_request; }

private:
    ResourceLoader(ResourceLoaderHost*, Resource*, const ResourceLoaderOptions&);

    void init(const ResourceRequest&);
    void requestSynchronously();

    void didFinishLoadingOnePart(double finishTime, int64_t encodedDataLength);

    bool responseNeedsAccessControlCheck() const;

    ResourceRequest& applyOptions(ResourceRequest&) const;

    OwnPtr<blink::WebURLLoader> m_loader;
    RefPtrWillBeMember<ResourceLoaderHost> m_host;

    ResourceRequest m_request;
    ResourceRequest m_originalRequest; // Before redirects.

    bool m_notifiedLoadComplete;

    bool m_defersLoading;
    OwnPtr<ResourceRequest> m_fallbackRequestForServiceWorker;
    ResourceRequest m_deferredRequest;
    ResourceLoaderOptions m_options;

    enum ResourceLoaderState {
        Initialized,
        Finishing,
        Terminated
    };

    enum ConnectionState {
        ConnectionStateNew,
        ConnectionStateStarted,
        ConnectionStateReceivedResponse,
        ConnectionStateReceivingData,
        ConnectionStateFinishedLoading,
        ConnectionStateCanceled,
        ConnectionStateFailed,
    };

    RawPtrWillBeMember<Resource> m_resource;
    ResourceLoaderState m_state;

    // Used for sanity checking to make sure we don't experience illegal state
    // transitions.
    ConnectionState m_connectionState;

    RefPtr<EventRacerLog> m_log;
    EventAction *m_eventAction;
    void doCancel(const ResourceError&);
};

}

#endif
