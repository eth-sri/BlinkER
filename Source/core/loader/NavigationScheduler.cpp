/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2009 Adam Barth. All rights reserved.
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

#include "config.h"
#include "core/loader/NavigationScheduler.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/events/Event.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLFormElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FormState.h"
#include "core/loader/FormSubmission.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/FrameLoaderStateMachine.h"
#include "core/page/Page.h"
#include "platform/SharedBuffer.h"
#include "platform/UserGestureIndicator.h"
#include "wtf/CurrentTime.h"

namespace blink {

unsigned NavigationDisablerForBeforeUnload::s_navigationDisableCount = 0;

class ScheduledNavigation {
    WTF_MAKE_NONCOPYABLE(ScheduledNavigation); WTF_MAKE_FAST_ALLOCATED;
public:
    ScheduledNavigation(double delay, bool lockBackForwardList, bool isLocationChange)
        : m_delay(delay)
        , m_lockBackForwardList(lockBackForwardList)
        , m_isLocationChange(isLocationChange)
        , m_wasUserGesture(UserGestureIndicator::processingUserGesture())
    {
        if (m_wasUserGesture)
            m_userGestureToken = UserGestureIndicator::currentToken();
    }
    virtual ~ScheduledNavigation() { }

    virtual void fire(LocalFrame*) = 0;

    virtual bool shouldStartTimer(LocalFrame*) { return true; }

    double delay() const { return m_delay; }
    bool lockBackForwardList() const { return m_lockBackForwardList; }
    bool isLocationChange() const { return m_isLocationChange; }
    PassOwnPtr<UserGestureIndicator> createUserGestureIndicator()
    {
        if (m_wasUserGesture &&  m_userGestureToken)
            return adoptPtr(new UserGestureIndicator(m_userGestureToken));
        return adoptPtr(new UserGestureIndicator(DefinitelyNotProcessingUserGesture));
    }

protected:
    void clearUserGesture() { m_wasUserGesture = false; }

private:
    double m_delay;
    bool m_lockBackForwardList;
    bool m_isLocationChange;
    bool m_wasUserGesture;
    RefPtr<UserGestureToken> m_userGestureToken;
};

class ScheduledURLNavigation : public ScheduledNavigation {
protected:
    ScheduledURLNavigation(double delay, Document* originDocument, const String& url, bool lockBackForwardList, bool isLocationChange)
        : ScheduledNavigation(delay, lockBackForwardList, isLocationChange)
        , m_originDocument(originDocument)
        , m_url(url)
        , m_shouldCheckMainWorldContentSecurityPolicy(CheckContentSecurityPolicy)
    {
        if (ContentSecurityPolicy::shouldBypassMainWorld(originDocument))
            m_shouldCheckMainWorldContentSecurityPolicy = DoNotCheckContentSecurityPolicy;
    }

    virtual void fire(LocalFrame* frame) override
    {
        OwnPtr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        FrameLoadRequest request(m_originDocument.get(), m_url, "_self", m_shouldCheckMainWorldContentSecurityPolicy);
        request.setLockBackForwardList(lockBackForwardList());
        request.setClientRedirect(ClientRedirect);
        frame->loader().load(request);
    }

    Document* originDocument() const { return m_originDocument.get(); }
    String url() const { return m_url; }

private:
    RefPtrWillBePersistent<Document> m_originDocument;
    String m_url;
    ContentSecurityPolicyDisposition m_shouldCheckMainWorldContentSecurityPolicy;
};

class ScheduledRedirect final : public ScheduledURLNavigation {
public:
    ScheduledRedirect(double delay, Document* originDocument, const String& url, bool lockBackForwardList)
        : ScheduledURLNavigation(delay, originDocument, url, lockBackForwardList, false)
    {
        clearUserGesture();
    }

    virtual bool shouldStartTimer(LocalFrame* frame) override { return frame->document()->loadEventFinished(); }

    virtual void fire(LocalFrame* frame) override
    {
        OwnPtr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        FrameLoadRequest request(originDocument(), url(), "_self");
        request.setLockBackForwardList(lockBackForwardList());
        if (equalIgnoringFragmentIdentifier(frame->document()->url(), request.resourceRequest().url()))
            request.resourceRequest().setCachePolicy(ReloadIgnoringCacheData);
        request.setClientRedirect(ClientRedirect);
        frame->loader().load(request);
    }
};

class ScheduledLocationChange final : public ScheduledURLNavigation {
public:
    ScheduledLocationChange(Document* originDocument, const String& url, bool lockBackForwardList)
        : ScheduledURLNavigation(0.0, originDocument, url, lockBackForwardList, !protocolIsJavaScript(url)) { }
};

class ScheduledReload final : public ScheduledNavigation {
public:
    ScheduledReload()
        : ScheduledNavigation(0.0, true, true)
    {
    }

    virtual void fire(LocalFrame* frame) override
    {
        OwnPtr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        frame->loader().reload(NormalReload, KURL(), ClientRedirect);
    }
};

class ScheduledPageBlock final : public ScheduledURLNavigation {
public:
    ScheduledPageBlock(Document* originDocument, const String& url)
        : ScheduledURLNavigation(0.0, originDocument, url, true, true)
    {
    }

    virtual void fire(LocalFrame* frame) override
    {
        OwnPtr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        SubstituteData substituteData(SharedBuffer::create(), "text/plain", "UTF-8", KURL(), ForceSynchronousLoad);
        FrameLoadRequest request(originDocument(), url(), substituteData);
        request.setLockBackForwardList(true);
        request.setClientRedirect(ClientRedirect);
        frame->loader().load(request);
    }
};

class ScheduledFormSubmission final : public ScheduledNavigation {
public:
    ScheduledFormSubmission(PassRefPtrWillBeRawPtr<FormSubmission> submission, bool lockBackForwardList)
        : ScheduledNavigation(0, lockBackForwardList, true)
        , m_submission(submission)
    {
        ASSERT(m_submission->state());
    }

    virtual void fire(LocalFrame* frame) override
    {
        OwnPtr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        FrameLoadRequest frameRequest(m_submission->state()->sourceDocument());
        m_submission->populateFrameLoadRequest(frameRequest);
        frameRequest.setLockBackForwardList(lockBackForwardList());
        frameRequest.setTriggeringEvent(m_submission->event());
        frameRequest.setFormState(m_submission->state());
        frame->loader().load(frameRequest);
    }

private:
    RefPtrWillBePersistent<FormSubmission> m_submission;
};

NavigationScheduler::NavigationScheduler(LocalFrame* frame)
    : m_frame(frame)
    , m_timer(this, &NavigationScheduler::timerFired)
{
}

NavigationScheduler::~NavigationScheduler()
{
}

bool NavigationScheduler::locationChangePending()
{
    return m_redirect && m_redirect->isLocationChange();
}

inline bool NavigationScheduler::shouldScheduleReload() const
{
    return m_frame->page() && NavigationDisablerForBeforeUnload::isNavigationAllowed();
}

inline bool NavigationScheduler::shouldScheduleNavigation(const String& url) const
{
    return m_frame->page() && (protocolIsJavaScript(url) || NavigationDisablerForBeforeUnload::isNavigationAllowed());
}

void NavigationScheduler::scheduleRedirect(double delay, const String& url)
{
    if (!shouldScheduleNavigation(url))
        return;
    if (delay < 0 || delay > INT_MAX / 1000)
        return;
    if (url.isEmpty())
        return;

    // We want a new back/forward list item if the refresh timeout is > 1 second.
    if (!m_redirect || delay <= m_redirect->delay())
        schedule(adoptPtr(new ScheduledRedirect(delay, m_frame->document(), url, delay <= 1)));
}

bool NavigationScheduler::mustLockBackForwardList(LocalFrame* targetFrame)
{
    // Non-user navigation before the page has finished firing onload should not create a new back/forward item.
    // See https://webkit.org/b/42861 for the original motivation for this.
    if (!UserGestureIndicator::processingUserGesture() && !targetFrame->document()->loadEventFinished())
        return true;

    // From the HTML5 spec for location.assign():
    //  "If the browsing context's session history contains only one Document,
    //   and that was the about:blank Document created when the browsing context
    //   was created, then the navigation must be done with replacement enabled."
    if (!targetFrame->loader().stateMachine()->committedMultipleRealLoads()
        && equalIgnoringCase(targetFrame->document()->url(), blankURL()))
        return true;

    // Navigation of a subframe during loading of an ancestor frame does not create a new back/forward item.
    // The definition of "during load" is any time before all handlers for the load event have been run.
    // See https://bugs.webkit.org/show_bug.cgi?id=14957 for the original motivation for this.
    Frame* parentFrame = targetFrame->tree().parent();
    return parentFrame && parentFrame->isLocalFrame() && !toLocalFrame(parentFrame)->loader().allAncestorsAreComplete();
}

void NavigationScheduler::scheduleLocationChange(Document* originDocument, const String& url, bool lockBackForwardList)
{
    if (!shouldScheduleNavigation(url))
        return;
    if (url.isEmpty())
        return;

    lockBackForwardList = lockBackForwardList || mustLockBackForwardList(m_frame);

    // If the URL we're going to navigate to is the same as the current one, except for the
    // fragment part, we don't need to schedule the location change. We'll skip this
    // optimization for cross-origin navigations to minimize the navigator's ability to
    // execute timing attacks.
    if (originDocument->securityOrigin()->canAccess(m_frame->document()->securityOrigin())) {
        KURL parsedURL(ParsedURLString, url);
        if (parsedURL.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(m_frame->document()->url(), parsedURL)) {
            FrameLoadRequest request(originDocument, m_frame->document()->completeURL(url), "_self");
            request.setLockBackForwardList(lockBackForwardList);
            if (lockBackForwardList)
                request.setClientRedirect(ClientRedirect);
            m_frame->loader().load(request);
            return;
        }
    }

    schedule(adoptPtr(new ScheduledLocationChange(originDocument, url, lockBackForwardList)));
}

void NavigationScheduler::schedulePageBlock(Document* originDocument)
{
    ASSERT(m_frame->page());
    const KURL& url = m_frame->document()->url();
    schedule(adoptPtr(new ScheduledPageBlock(originDocument, url)));
}

void NavigationScheduler::scheduleFormSubmission(PassRefPtrWillBeRawPtr<FormSubmission> submission)
{
    ASSERT(m_frame->page());
    schedule(adoptPtr(new ScheduledFormSubmission(submission, mustLockBackForwardList(m_frame))));
}

void NavigationScheduler::scheduleReload()
{
    if (!shouldScheduleReload())
        return;
    if (m_frame->document()->url().isEmpty())
        return;
    schedule(adoptPtr(new ScheduledReload));
}

void NavigationScheduler::timerFired(EventRacerTimer<NavigationScheduler>*)
{
    if (!m_frame->page())
        return;
    if (m_frame->page()->defersLoading()) {
        InspectorInstrumentation::frameClearedScheduledNavigation(m_frame);
        return;
    }

    RefPtrWillBeRawPtr<LocalFrame> protect(m_frame.get());

    OwnPtr<ScheduledNavigation> redirect(m_redirect.release());
    redirect->fire(m_frame);
    InspectorInstrumentation::frameClearedScheduledNavigation(m_frame);
}

void NavigationScheduler::schedule(PassOwnPtr<ScheduledNavigation> redirect)
{
    ASSERT(m_frame->page());

    // In a back/forward navigation, we sometimes restore history state to iframes, even though the state was generated
    // dynamically and JS will try to put something different in the iframe. In this case, we will load stale things
    // and/or confuse the JS when it shortly thereafter tries to schedule a location change. Let the JS have its way.
    // FIXME: This check seems out of place.
    if (!m_frame->loader().stateMachine()->committedFirstRealDocumentLoad() && m_frame->loader().provisionalDocumentLoader()) {
        RefPtrWillBeRawPtr<LocalFrame> protect(m_frame.get());
        m_frame->loader().provisionalDocumentLoader()->stopLoading();
        if (!m_frame->host())
            return;
    }

    cancel();
    m_redirect = redirect;
    startTimer();
}

void NavigationScheduler::startTimer()
{
    if (!m_redirect)
        return;

    ASSERT(m_frame->page());
    if (m_timer.isActive())
        return;
    if (!m_redirect->shouldStartTimer(m_frame))
        return;

    m_timer.startOneShot(m_redirect->delay(), FROM_HERE);
    InspectorInstrumentation::frameScheduledNavigation(m_frame, m_redirect->delay());
}

void NavigationScheduler::cancel()
{
    if (m_timer.isActive())
        InspectorInstrumentation::frameClearedScheduledNavigation(m_frame);
    m_timer.stop();
    m_redirect.clear();
}

void NavigationScheduler::trace(Visitor* visitor)
{
    visitor->trace(m_frame);
}

} // namespace blink
