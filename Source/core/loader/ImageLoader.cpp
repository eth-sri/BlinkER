/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010 Apple Inc. All rights reserved.
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
#include "core/loader/ImageLoader.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/IncrementLoadEventDelayCount.h"
#include "core/dom/Microtask.h"
#include "core/eventracer/EventRacerContext.h"
#include "core/eventracer/EventRacerLog.h"
#include "core/events/Event.h"
#include "core/events/EventSender.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderVideo.h"
#include "core/rendering/svg/RenderSVGImage.h"
#include "platform/Logging.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

static ImageEventSender& loadEventSender()
{
    DEFINE_STATIC_LOCAL(ImageEventSender, sender, (EventTypeNames::load));
    return sender;
}

static ImageEventSender& errorEventSender()
{
    DEFINE_STATIC_LOCAL(ImageEventSender, sender, (EventTypeNames::error));
    return sender;
}

static inline bool pageIsBeingDismissed(Document* document)
{
    return document->pageDismissalEventBeingDispatched() != Document::NoDismissal;
}

static ImageLoader::BypassMainWorldBehavior shouldBypassMainWorldCSP(ImageLoader* loader)
{
    ASSERT(loader);
    ASSERT(loader->element());
    if (loader->element()->document().frame() && loader->element()->document().frame()->script().shouldBypassMainWorldCSP())
        return ImageLoader::BypassMainWorldCSP;
    return ImageLoader::DoNotBypassMainWorldCSP;
}

class ImageLoader::Task : public blink::WebThread::Task {
public:
    static PassOwnPtr<Task> create(ImageLoader* loader, UpdateFromElementBehavior updateBehavior)
    {
        return adoptPtr(new Task(loader, updateBehavior));
    }

    Task(ImageLoader* loader, UpdateFromElementBehavior updateBehavior)
        : m_loader(loader)
        , m_shouldBypassMainWorldCSP(shouldBypassMainWorldCSP(loader))
        , m_weakFactory(this)
        , m_updateBehavior(updateBehavior)
        , m_startAction(0)
    {
        m_log = EventRacerContext::getLog();
        if (m_log && m_log->hasAction()) {
            m_startAction = m_log->getCurrentAction();
            m_startAction->willDeferJoin();
        }
    }

    virtual void run() override
    {
        if (m_loader) {
            if (m_log && m_startAction && !m_log->hasAction()) {
                EventRacerContext ctx(m_log);
                EventAction *action = m_log->createEventAction();
                EventActionScope act(action);
                m_log->join(m_startAction, action);
                OperationScope op("img-ldr:tsk-run");
                m_loader->doUpdateFromElement(m_shouldBypassMainWorldCSP, m_updateBehavior);
                m_loader->updateLoadDelay();
            } else {
                m_loader->doUpdateFromElement(m_shouldBypassMainWorldCSP, m_updateBehavior);
                m_loader->updateLoadDelay();
            }
        }
    }

    void clearLoader()
    {
        m_loader = 0;
    }

    WeakPtr<Task> createWeakPtr()
    {
        return m_weakFactory.createWeakPtr();
    }

private:
    ImageLoader* m_loader;
    BypassMainWorldBehavior m_shouldBypassMainWorldCSP;
    WeakPtrFactory<Task> m_weakFactory;
    UpdateFromElementBehavior m_updateBehavior;
    RefPtr<EventRacerLog> m_log;
    EventAction *m_startAction;
};

ImageLoader::ImageLoader(Element* element)
    : m_element(element)
    , m_image(0)
    , m_derefElementTimer(this, &ImageLoader::timerFired)
    , m_hasPendingLoadEvent(false)
    , m_hasPendingErrorEvent(false)
    , m_imageComplete(true)
    , m_loadingImageDocument(false)
    , m_elementIsProtected(false)
    , m_suppressErrorEvents(false)
    , m_highPriorityClientCount(0)
    , m_action()
{
    WTF_LOG(Timers, "new ImageLoader %p", this);
}

ImageLoader::~ImageLoader()
{
    WTF_LOG(Timers, "~ImageLoader %p; m_hasPendingLoadEvent=%d, m_hasPendingErrorEvent=%d",
        this, m_hasPendingLoadEvent, m_hasPendingErrorEvent);

    if (m_pendingTask)
        m_pendingTask->clearLoader();

    if (m_image)
        m_image->removeClient(this);

    ASSERT(m_hasPendingLoadEvent || !loadEventSender().hasPendingEvents(this));
    if (m_hasPendingLoadEvent)
        cancelPendingLoadEvent();

    ASSERT(m_hasPendingErrorEvent || !errorEventSender().hasPendingEvents(this));
    if (m_hasPendingErrorEvent)
        cancelPendingErrorEvent();
}

void ImageLoader::trace(Visitor* visitor)
{
    visitor->trace(m_element);
}

void ImageLoader::setImage(ImageResource* newImage)
{
    setImageWithoutConsideringPendingLoadEvent(newImage);

    // Only consider updating the protection ref-count of the Element immediately before returning
    // from this function as doing so might result in the destruction of this ImageLoader.
    updatedHasPendingEvent();
}

void ImageLoader::setImageWithoutConsideringPendingLoadEvent(ImageResource* newImage)
{
    ASSERT(m_failedLoadURL.isEmpty());
    ImageResource* oldImage = m_image.get();
    if (newImage != oldImage) {
        sourceImageChanged();
        m_image = newImage;
        if (m_hasPendingLoadEvent)
            cancelPendingLoadEvent();
        if (m_hasPendingErrorEvent)
            cancelPendingErrorEvent();
        m_imageComplete = true;
        if (newImage)
            newImage->addClient(this);
        if (oldImage)
            oldImage->removeClient(this);
    }

    if (RenderImageResource* imageResource = renderImageResource())
        imageResource->resetAnimation();
}

static void configureRequest(FetchRequest& request, ImageLoader::BypassMainWorldBehavior bypassBehavior, Element& element)
{
    if (bypassBehavior == ImageLoader::BypassMainWorldCSP)
        request.setContentSecurityCheck(DoNotCheckContentSecurityPolicy);

    AtomicString crossOriginMode = element.fastGetAttribute(HTMLNames::crossoriginAttr);
    if (!crossOriginMode.isNull())
        request.setCrossOriginAccessControl(element.document().securityOrigin(), crossOriginMode);
}

ResourcePtr<ImageResource> ImageLoader::createImageResourceForImageDocument(Document& document, FetchRequest& request)
{
    bool autoLoadOtherImages = document.fetcher()->autoLoadImages();
    document.fetcher()->setAutoLoadImages(false);
    ResourcePtr<ImageResource> newImage = new ImageResource(request.resourceRequest());
    newImage->setLoading(true);
    document.fetcher()->m_documentResources.set(newImage->url(), newImage.get());
    document.fetcher()->setAutoLoadImages(autoLoadOtherImages);
    return newImage;
}

inline void ImageLoader::dispatchErrorEvent()
{
    if (EventRacerContext::getLog()) {
        m_log = EventRacerContext::getLog();
        m_action[ERROR] = m_log->getCurrentAction();
    }

    m_hasPendingErrorEvent = true;
    errorEventSender().dispatchEventSoon(this);
}

inline void ImageLoader::crossSiteOrCSPViolationOccurred(AtomicString imageSourceURL)
{
    m_failedLoadURL = imageSourceURL;
}

inline void ImageLoader::clearFailedLoadURL()
{
    m_failedLoadURL = AtomicString();
}

inline void ImageLoader::enqueueImageLoadingMicroTask(UpdateFromElementBehavior updateBehavior)
{
    OwnPtr<Task> task = Task::create(this, updateBehavior);
    m_pendingTask = task->createWeakPtr();
    Microtask::enqueueMicrotask(task.release());
}

inline void ImageLoader::incrementLoadDelay()
{
    ASSERT(!m_loadDelayCounter);
    m_loadDelayCounter = IncrementLoadEventDelayCount::create(m_element->document());
}

inline void ImageLoader::decrementLoadDelay()
{
    OwnPtr<IncrementLoadEventDelayCount> loadDelayCounter;
    loadDelayCounter.swap(m_loadDelayCounter);
}

inline void ImageLoader::updateLoadDelay() {
    if (m_pendingTask || m_hasPendingLoadEvent || m_hasPendingErrorEvent) {
        if (!m_loadDelayCounter)
            incrementLoadDelay();
    } else {
        decrementLoadDelay();
    }
}

void ImageLoader::doUpdateFromElement(BypassMainWorldBehavior bypassBehavior, UpdateFromElementBehavior updateBehavior)
{
    // FIXME: According to
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/embedded-content.html#the-img-element:the-img-element-55
    // When "update image" is called due to environment changes and the load fails, onerror should not be called.
    // That is currently not the case.
    //
    // We don't need to call clearLoader here: Either we were called from the
    // task, or our caller updateFromElement cleared the task's loader (and set
    // m_pendingTask to null).
    m_pendingTask.clear();

    Document& document = m_element->document();
    if (!document.isActive())
        return;

    AtomicString imageSourceURL = m_element->imageSourceURL();
    KURL url = imageSourceToKURL(imageSourceURL);
    ResourcePtr<ImageResource> newImage = 0;
    RefPtrWillBeRawPtr<Element> protectElement(m_element.get());
    if (!url.isNull()) {
        // Unlike raw <img>, we block mixed content inside of <picture> or <img srcset>.
        ResourceLoaderOptions resourceLoaderOptions = ResourceFetcher::defaultResourceOptions();
        ResourceRequest resourceRequest(url);
        resourceRequest.setFetchCredentialsMode(WebURLRequest::FetchCredentialsModeSameOrigin);
        if (isHTMLPictureElement(element()->parentNode()) || !element()->fastGetAttribute(HTMLNames::srcsetAttr).isNull()) {
            resourceLoaderOptions.mixedContentBlockingTreatment = TreatAsActiveContent;
            resourceRequest.setRequestContext(WebURLRequest::RequestContextImageSet);
        }
        FetchRequest request(resourceRequest, element()->localName(), resourceLoaderOptions);
        configureRequest(request, bypassBehavior, *m_element);

        if (m_loadingImageDocument)
            newImage = createImageResourceForImageDocument(document, request);
        else
            newImage = document.fetcher()->fetchImage(request);

        if (!newImage && !pageIsBeingDismissed(&document)) {
            crossSiteOrCSPViolationOccurred(imageSourceURL);
            dispatchErrorEvent();
        } else {
            clearFailedLoadURL();
        }
    } else {
        if (!imageSourceURL.isNull()) {
            // Fire an error event if the url string is not empty, but the KURL is.
            dispatchErrorEvent();
        }
        noImageResourceToLoad();
    }

    ImageResource* oldImage = m_image.get();
    if (newImage != oldImage) {
        sourceImageChanged();

        if (m_hasPendingLoadEvent)
            cancelPendingLoadEvent();

        // Cancel error events that belong to the previous load, which is now cancelled by changing the src attribute.
        // If newImage is null and m_hasPendingErrorEvent is true, we know the error event has been just posted by
        // this load and we should not cancel the event.
        // FIXME: If both previous load and this one got blocked with an error, we can receive one error event instead of two.
        if (m_hasPendingErrorEvent && newImage)
            cancelPendingErrorEvent();

        m_image = newImage;
        m_hasPendingLoadEvent = newImage;
        m_imageComplete = !newImage;

        updateRenderer();
        // If newImage exists and is cached, addClient() will result in the load event
        // being queued to fire. Ensure this happens after beforeload is dispatched.
        if (newImage)
            newImage->addClient(this);

        if (oldImage)
            oldImage->removeClient(this);
    } else if (updateBehavior == UpdateSizeChanged && m_element->renderer() && m_element->renderer()->isImage()) {
        toRenderImage(m_element->renderer())->intrinsicSizeChanged();
    }

    if (RenderImageResource* imageResource = renderImageResource())
        imageResource->resetAnimation();

    // Only consider updating the protection ref-count of the Element immediately before returning
    // from this function as doing so might result in the destruction of this ImageLoader.
    updatedHasPendingEvent();
}

void ImageLoader::updateFromElement(UpdateFromElementBehavior updateBehavior)
{
    AtomicString imageSourceURL = m_element->imageSourceURL();
    m_suppressErrorEvents = (updateBehavior == UpdateSizeChanged);

    if (updateBehavior == UpdateIgnorePreviousError)
        clearFailedLoadURL();

    if (!m_failedLoadURL.isEmpty() && imageSourceURL == m_failedLoadURL)
        return;

    // If we have a pending task, we have to clear it -- either we're
    // now loading immediately, or we need to reset the task's state.
    if (m_pendingTask) {
        m_pendingTask->clearLoader();
        m_pendingTask.clear();
    }

    KURL url = imageSourceToKURL(imageSourceURL);
    if (shouldLoadImmediately(url)) {
        doUpdateFromElement(DoNotBypassMainWorldCSP, updateBehavior);
        updateLoadDelay();
        return;
    }

    enqueueImageLoadingMicroTask(updateBehavior);
    updateLoadDelay();
}

KURL ImageLoader::imageSourceToKURL(AtomicString imageSourceURL) const
{
    KURL url;

    // Don't load images for inactive documents. We don't want to slow down the
    // raw HTML parsing case by loading images we don't intend to display.
    Document& document = m_element->document();
    if (!document.isActive())
        return url;

    // Do not load any image if the 'src' attribute is missing or if it is
    // an empty string.
    if (!imageSourceURL.isNull() && !stripLeadingAndTrailingHTMLSpaces(imageSourceURL).isEmpty())
        url = document.completeURL(sourceURI(imageSourceURL));
    return url;
}

bool ImageLoader::shouldLoadImmediately(const KURL& url) const
{
    // We force any image loads which might require alt content through the asynchronous path so that we can add the shadow DOM
    // for the alt-text content when style recalc is over and DOM mutation is allowed again.
    if (!url.isNull()) {
        Resource* resource = memoryCache()->resourceForURL(url, m_element->document().fetcher()->getCacheIdentifier());
        if (resource && !resource->errorOccurred())
            return true;
    }
    return (m_loadingImageDocument || isHTMLObjectElement(m_element) || isHTMLEmbedElement(m_element) || url.protocolIsData());
}

void ImageLoader::notifyFinished(Resource* resource)
{
    WTF_LOG(Timers, "ImageLoader::notifyFinished %p; m_hasPendingLoadEvent=%d",
        this, m_hasPendingLoadEvent);

    ASSERT(m_failedLoadURL.isEmpty());
    ASSERT(resource == m_image.get());

    m_imageComplete = true;
    updateRenderer();

    if (!m_hasPendingLoadEvent)
        return;

    if (resource->errorOccurred()) {
        cancelPendingLoadEvent();

        if (resource->resourceError().isAccessCheck())
            crossSiteOrCSPViolationOccurred(AtomicString(resource->resourceError().failingURL()));

        // The error event should not fire if the image data update is a result of environment change.
        // https://html.spec.whatwg.org/multipage/embedded-content.html#the-img-element:the-img-element-55
        if (!m_suppressErrorEvents)
            dispatchErrorEvent();

        // Only consider updating the protection ref-count of the Element immediately before returning
        // from this function as doing so might result in the destruction of this ImageLoader.
        updatedHasPendingEvent();
        return;
    }
    if (resource->wasCanceled()) {
        m_hasPendingLoadEvent = false;
        // Only consider updating the protection ref-count of the Element immediately before returning
        // from this function as doing so might result in the destruction of this ImageLoader.
        updatedHasPendingEvent();
        return;
    }

    if (EventRacerContext::getLog()) {
        m_log = EventRacerContext::getLog();
        m_action[LOAD] = m_log->getCurrentAction();
    }

    loadEventSender().dispatchEventSoon(this);
}

void ImageLoader::cancelPendingErrorEvent() {
    errorEventSender().cancelEvent(this);
    m_action[ERROR] = nullptr;
    m_hasPendingErrorEvent = false;
}

void ImageLoader::cancelPendingLoadEvent() {
    loadEventSender().cancelEvent(this);
    m_action[LOAD] = nullptr;
    m_hasPendingLoadEvent = false;
}

RenderImageResource* ImageLoader::renderImageResource()
{
    RenderObject* renderer = m_element->renderer();

    if (!renderer)
        return 0;

    // We don't return style generated image because it doesn't belong to the ImageLoader.
    // See <https://bugs.webkit.org/show_bug.cgi?id=42840>
    if (renderer->isImage() && !static_cast<RenderImage*>(renderer)->isGeneratedContent())
        return toRenderImage(renderer)->imageResource();

    if (renderer->isSVGImage())
        return toRenderSVGImage(renderer)->imageResource();

    if (renderer->isVideo())
        return toRenderVideo(renderer)->imageResource();

    return 0;
}

void ImageLoader::updateRenderer()
{
    RenderImageResource* imageResource = renderImageResource();

    if (!imageResource)
        return;

    // Only update the renderer if it doesn't have an image or if what we have
    // is a complete image.  This prevents flickering in the case where a dynamic
    // change is happening between two images.
    ImageResource* cachedImage = imageResource->cachedImage();
    if (m_image != cachedImage && (m_imageComplete || !cachedImage))
        imageResource->setImageResource(m_image.get());
}

void ImageLoader::updatedHasPendingEvent()
{
    // If an Element that does image loading is removed from the DOM the load/error event for the image is still observable.
    // As long as the ImageLoader is actively loading, the Element itself needs to be ref'ed to keep it from being
    // destroyed by DOM manipulation or garbage collection.
    // If such an Element wishes for the load to stop when removed from the DOM it needs to stop the ImageLoader explicitly.
    bool wasProtected = m_elementIsProtected;
    m_elementIsProtected = m_hasPendingLoadEvent || m_hasPendingErrorEvent;
    if (wasProtected == m_elementIsProtected)
        return;

    if (m_elementIsProtected) {
        if (m_derefElementTimer.isActive())
            m_derefElementTimer.stop();
        else
            m_keepAlive = m_element;
    } else {
        ASSERT(!m_derefElementTimer.isActive());
        m_derefElementTimer.startOneShot(0, FROM_HERE);
    }
}

void ImageLoader::timerFired(Timer<ImageLoader>*)
{
    m_keepAlive.clear();
}

void ImageLoader::dispatchPendingEvent(ImageEventSender* eventSender)
{
    WTF_LOG(Timers, "ImageLoader::dispatchPendingEvent %p", this);
    ASSERT(eventSender == &loadEventSender() || eventSender == &errorEventSender());
    const AtomicString& eventType = eventSender->eventType();
    if (eventType == EventTypeNames::load)
        dispatchPendingLoadEvent();
    if (eventType == EventTypeNames::error)
        dispatchPendingErrorEvent();
}

void ImageLoader::dispatchPendingLoadEvent()
{
    if (!m_hasPendingLoadEvent)
        return;
    if (!m_image)
        return;

    m_hasPendingLoadEvent = false;

    if (m_log) {
        ASSERT(!m_log->hasAction());
        EventRacerContext ctx(m_log);
        EventActionScope act(m_log->createEventAction());
        m_log->join(m_action[LOAD], m_log->getCurrentAction());

        if (element()->document().frame())
            dispatchLoadEvent();
        decrementLoadDelay();
    } else {
        if (element()->document().frame())
            dispatchLoadEvent();
        decrementLoadDelay();
    }

    // Only consider updating the protection ref-count of the Element immediately before returning
    // from this function as doing so might result in the destruction of this ImageLoader.
    updatedHasPendingEvent();
}

void ImageLoader::dispatchPendingErrorEvent()
{
    if (!m_hasPendingErrorEvent)
        return;
    m_hasPendingErrorEvent = false;

    if (m_log) {
        ASSERT(!m_log->hasAction());
        EventRacerContext ctx(m_log);
        EventActionScope act(m_log->createEventAction());
        m_log->join(m_action[ERROR], m_log->getCurrentAction());

        if (element()->document().frame())
            element()->dispatchEvent(Event::create(EventTypeNames::error));
        decrementLoadDelay();
    } else {
        if (element()->document().frame())
            element()->dispatchEvent(Event::create(EventTypeNames::error));
        decrementLoadDelay();
    }

    // Only consider updating the protection ref-count of the Element immediately before returning
    // from this function as doing so might result in the destruction of this ImageLoader.
    updatedHasPendingEvent();
}

void ImageLoader::addClient(ImageLoaderClient* client)
{
    if (client->requestsHighLiveResourceCachePriority()) {
        if (m_image && !m_highPriorityClientCount++)
            memoryCache()->updateDecodedResource(m_image.get(), UpdateForPropertyChange, MemoryCacheLiveResourcePriorityHigh);
    }
#if ENABLE(OILPAN)
    m_clients.add(client, adoptPtr(new ImageLoaderClientRemover(*this, *client)));
#else
    m_clients.add(client);
#endif
}

void ImageLoader::willRemoveClient(ImageLoaderClient& client)
{
    if (client.requestsHighLiveResourceCachePriority()) {
        ASSERT(m_highPriorityClientCount);
        m_highPriorityClientCount--;
        if (m_image && !m_highPriorityClientCount)
            memoryCache()->updateDecodedResource(m_image.get(), UpdateForPropertyChange, MemoryCacheLiveResourcePriorityLow);
    }
}

void ImageLoader::removeClient(ImageLoaderClient* client)
{
    willRemoveClient(*client);
    m_clients.remove(client);
}

void ImageLoader::dispatchPendingLoadEvents()
{
    loadEventSender().dispatchPendingEvents();
}

void ImageLoader::dispatchPendingErrorEvents()
{
    errorEventSender().dispatchPendingEvents();
}

void ImageLoader::elementDidMoveToNewDocument()
{
    if (m_loadDelayCounter)
        m_loadDelayCounter->documentChanged(m_element->document());
    clearFailedLoadURL();
    setImage(0);
}

void ImageLoader::sourceImageChanged()
{
#if ENABLE(OILPAN)
    for (auto& client : m_clients)
        client.key->notifyImageSourceChanged();
#else
    for (auto& client : m_clients) {
        ImageLoaderClient* handle = client;
        handle->notifyImageSourceChanged();
    }
#endif
}

#if ENABLE(OILPAN)
ImageLoader::ImageLoaderClientRemover::~ImageLoaderClientRemover()
{
    m_loader.willRemoveClient(m_client);
}
#endif
}
