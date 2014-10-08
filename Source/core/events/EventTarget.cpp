/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "core/events/EventTarget.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/NoEventDispatchAssertion.h"
#include "core/editing/Editor.h"
#include "core/eventracer/EventRacerContext.h"
#include "core/eventracer/EventRacerLog.h"
#include "core/events/Event.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/UseCounter.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/Atomics.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"

using namespace WTF;

namespace blink {

volatile unsigned int EventTargetData::nextSerial;

EventTargetData::EventTargetData()
{
    serial = WTF::atomicIncrement(reinterpret_cast<volatile int *>(&nextSerial));
}

EventTargetData::~EventTargetData()
{
}

EventTarget::EventTarget()
{
    ScriptWrappable::init(this);
}

EventTarget::~EventTarget()
{
}

EventTargetWithInlineData::EventTargetWithInlineData()
{
    m_log = EventRacerContext::getLog();
    if (m_log && m_log->hasAction()) {
        m_creatorAction = m_log->getCurrentAction();
        m_creatorAction->willDeferJoin();
    } else {
        m_creatorAction = 0;
    }
}

EventTargetWithInlineData::~EventTargetWithInlineData() {}

void EventTargetWithInlineData::setCreatorEventRacerContext(PassRefPtr<EventRacerLog> log, EventAction *act)
{
    m_log = log;
    m_creatorAction = act;
    m_creatorAction->willDeferJoin();
}

PassRefPtr<EventRacerLog> EventTargetWithInlineData::getCreatorEventRacerLog() const
{
    return m_log;
}

Node* EventTarget::toNode()
{
    return 0;
}

LocalDOMWindow* EventTarget::toDOMWindow()
{
    return 0;
}

MessagePort* EventTarget::toMessagePort()
{
    return 0;
}

inline LocalDOMWindow* EventTarget::executingWindow()
{
    if (ExecutionContext* context = executionContext())
        return context->executingWindow();
    return 0;
}

namespace {

void eventTargetAccess(PassRefPtr<EventRacerLog> lg, Operation::Type op, EventTarget *target,
                       const AtomicString &eventType)
{
    RefPtr<EventRacerLog> log = lg;
    Node *node = target->toNode();
    String name = node ? node->nodeName() : "";
    log->logOperation(log->getCurrentAction(), op,
                      log->getStrings(VAR_STRINGS).putf("%s[0x%x].%s",
                                                        name.isEmpty() ? "" : name.ascii().data(),
                                                        target->getSerial(),
                                                        eventType.string().ascii().data()));
}

}


bool EventTarget::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> lstnr, bool useCapture)
{
    // Note that an attempt to add a null listener is still recorded as a write
    // as it may race with another read or write and the listener being null
    // might well be the result of an unrelated error.
    RefPtr<EventListener> listener = lstnr;
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    if (log && log->hasAction()) {
        OperationScope scope("addEventListener");
        eventTargetAccess(log, Operation::WRITE_MEMORY, this, eventType);
        log->logOperation(log->getCurrentAction(), Operation::MEMORY_VALUE,
                          log->getStrings(VALUE_STRINGS).putf("Event[0x%x]",
                                                              !listener ? 0 : listener->getSerial()));
    }

    // FIXME: listener null check should throw TypeError (and be done in
    // generated bindings), but breaks legacy content. http://crbug.com/249598
    if (!listener)
        return false;

    V8DOMActivityLogger* activityLogger = V8DOMActivityLogger::currentActivityLoggerIfIsolatedWorld();
    if (activityLogger) {
        Vector<String> argv;
        argv.append(toNode() ? toNode()->nodeName() : interfaceName());
        argv.append(eventType);
        activityLogger->logEvent("blinkAddEventListener", argv.size(), argv.data());
    }
    return ensureEventTargetData().eventListenerMap.add(eventType, listener, useCapture);
}

bool EventTarget::removeEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    if (log && log->hasAction()) {
        OperationScope scope("removeEventListener");
        eventTargetAccess(log, Operation::WRITE_MEMORY, this, eventType);
        log->logOperation(log->getCurrentAction(), Operation::MEMORY_VALUE, "undefined");
    }

    EventTargetData* d = eventTargetData();
    if (!d)
        return false;

    size_t indexOfRemovedListener;

    if (!d->eventListenerMap.remove(eventType, listener.get(), useCapture, indexOfRemovedListener))
        return false;

    // Notify firing events planning to invoke the listener at 'index' that
    // they have one less listener to invoke.
    if (!d->firingEventIterators)
        return true;
    for (size_t i = 0; i < d->firingEventIterators->size(); ++i) {
        FiringEventIterator& firingIterator = d->firingEventIterators->at(i);
        if (eventType != firingIterator.eventType)
            continue;

        if (indexOfRemovedListener >= firingIterator.end)
            continue;

        --firingIterator.end;
        if (indexOfRemovedListener <= firingIterator.iterator)
            --firingIterator.iterator;
    }

    return true;
}

bool EventTarget::setAttributeEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener)
{
    clearAttributeEventListener(eventType);
    if (!listener)
        return false;
    return addEventListener(eventType, listener, false);
}

EventListener* EventTarget::getAttributeEventListener(const AtomicString& eventType)
{
    const EventListenerVector& entry = getEventListeners(eventType);
    for (size_t i = 0; i < entry.size(); ++i) {
        EventListener* listener = entry[i].listener.get();
        if (listener->isAttribute() && listener->belongsToTheCurrentWorld())
            return listener;
    }
    return 0;
}

bool EventTarget::clearAttributeEventListener(const AtomicString& eventType)
{
    EventListener* listener = getAttributeEventListener(eventType);
    if (!listener)
        return false;
    return removeEventListener(eventType, listener, false);
}

bool EventTarget::dispatchEvent(PassRefPtrWillBeRawPtr<Event> event, ExceptionState& exceptionState)
{
    if (!event) {
        exceptionState.throwDOMException(InvalidStateError, "The event provided is null.");
        return false;
    }
    if (event->type().isEmpty()) {
        exceptionState.throwDOMException(InvalidStateError, "The event provided is uninitialized.");
        return false;
    }
    if (event->isBeingDispatched()) {
        exceptionState.throwDOMException(InvalidStateError, "The event is already being dispatched.");
        return false;
    }

    if (!executionContext())
        return false;

    return dispatchEvent(event);
}

bool EventTarget::dispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    event->setTarget(this);
    event->setCurrentTarget(this);
    event->setEventPhase(Event::AT_TARGET);
    bool defaultPrevented = fireEventListeners(event.get());
    event->setEventPhase(0);
    return defaultPrevented;
}

void EventTarget::uncaughtExceptionInEventHandler()
{
}

static const AtomicString& legacyType(const Event* event)
{
    if (event->type() == EventTypeNames::transitionend)
        return EventTypeNames::webkitTransitionEnd;

    if (event->type() == EventTypeNames::animationstart)
        return EventTypeNames::webkitAnimationStart;

    if (event->type() == EventTypeNames::animationend)
        return EventTypeNames::webkitAnimationEnd;

    if (event->type() == EventTypeNames::animationiteration)
        return EventTypeNames::webkitAnimationIteration;

    if (event->type() == EventTypeNames::wheel)
        return EventTypeNames::mousewheel;

    return emptyAtom;
}

void EventTarget::countLegacyEvents(const AtomicString& legacyTypeName, EventListenerVector* listenersVector, EventListenerVector* legacyListenersVector)
{
    UseCounter::Feature unprefixedFeature;
    UseCounter::Feature prefixedFeature;
    UseCounter::Feature prefixedAndUnprefixedFeature;
    if (legacyTypeName == EventTypeNames::webkitTransitionEnd) {
        prefixedFeature = UseCounter::PrefixedTransitionEndEvent;
        unprefixedFeature = UseCounter::UnprefixedTransitionEndEvent;
        prefixedAndUnprefixedFeature = UseCounter::PrefixedAndUnprefixedTransitionEndEvent;
    } else if (legacyTypeName == EventTypeNames::webkitAnimationEnd) {
        prefixedFeature = UseCounter::PrefixedAnimationEndEvent;
        unprefixedFeature = UseCounter::UnprefixedAnimationEndEvent;
        prefixedAndUnprefixedFeature = UseCounter::PrefixedAndUnprefixedAnimationEndEvent;
    } else if (legacyTypeName == EventTypeNames::webkitAnimationStart) {
        prefixedFeature = UseCounter::PrefixedAnimationStartEvent;
        unprefixedFeature = UseCounter::UnprefixedAnimationStartEvent;
        prefixedAndUnprefixedFeature = UseCounter::PrefixedAndUnprefixedAnimationStartEvent;
    } else if (legacyTypeName == EventTypeNames::webkitAnimationIteration) {
        prefixedFeature = UseCounter::PrefixedAnimationIterationEvent;
        unprefixedFeature = UseCounter::UnprefixedAnimationIterationEvent;
        prefixedAndUnprefixedFeature = UseCounter::PrefixedAndUnprefixedAnimationIterationEvent;
    } else {
        return;
    }

    if (LocalDOMWindow* executingWindow = this->executingWindow()) {
        if (legacyListenersVector) {
            if (listenersVector)
                UseCounter::count(executingWindow->document(), prefixedAndUnprefixedFeature);
            else
                UseCounter::count(executingWindow->document(), prefixedFeature);
        } else if (listenersVector) {
            UseCounter::count(executingWindow->document(), unprefixedFeature);
        }
    }
}

#ifndef NDEBUG
namespace {
    bool shouldIgnoreEventLogging(const AtomicString &type) {
        return type == "mousemove" || type == "mouseout" || type == "mouseover";
    }
}
#endif

bool EventTarget::fireEventListeners(Event* event)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    ASSERT(event && !event->type().isEmpty());

    RefPtrWillBeRawPtr<EventTarget> protect(this);

    EventTargetData* d = eventTargetData();
    if (!d)
        return true;

    EventListenerVector* legacyListenersVector = 0;
    AtomicString legacyTypeName = legacyType(event);
    if (!legacyTypeName.isEmpty())
        legacyListenersVector = d->eventListenerMap.find(legacyTypeName);

    EventListenerVector* listenersVector = d->eventListenerMap.find(event->type());
    if (!RuntimeEnabledFeatures::cssAnimationUnprefixedEnabled() && (event->type() == EventTypeNames::animationiteration || event->type() == EventTypeNames::animationend
        || event->type() == EventTypeNames::animationstart)
        // Some code out-there uses custom events to dispatch unprefixed animation events manually,
        // we can safely remove all this block when cssAnimationUnprefixedEnabled is always on, this
        // is really a special case. DO NOT ADD MORE EVENTS HERE.
        && event->interfaceName() != EventNames::CustomEvent)
        listenersVector = 0;

    RefPtr<EventRacerLog> log = getCreatorEventRacerLog();
    EventAction *prevAction = d->eventListenerMap.getEventAction(event->type());
    if (!prevAction)
        prevAction = getCreatorEventAction();

    EventRacerContext ctx(log);
    OwnPtr<EventActionScope> act;
    OwnPtr<OperationScope> op;
    EventAction *thisAction = 0;

#ifndef NDEBUG
    // In debug build, do not show certain events with no handler as it creates
    // a lot of clutter in the visualisation of the graph.
    if (shouldIgnoreEventLogging(event->type()) && !listenersVector && !legacyListenersVector)
        goto out;
#endif

    if (log) {
        if (log->hasAction())
            thisAction = log->getCurrentAction();
        else {
            thisAction = log->createEventAction();
            act = adoptPtr(new EventActionScope(thisAction));
        }
        if (prevAction && prevAction != thisAction)
            log->join(prevAction, thisAction);

        eventTargetAccess(log, Operation::READ_MEMORY, this, event->type());
        if (listenersVector || legacyListenersVector) {
            String ev = String::format("fire:%s @ %s", event->type().string().ascii().data(),
                                       event->eventPhase() == Event::AT_TARGET ? "TARGET"
                                       : event->eventPhase() == Event::CAPTURING_PHASE ? "CAPTURE"
                                       : "BUBBLE");
            op = adoptPtr(new OperationScope(ev));
        }
    }

    if (listenersVector) {
        fireEventListeners(event, d, *listenersVector);
    } else if (legacyListenersVector) {
        AtomicString unprefixedTypeName = event->type();
        event->setType(legacyTypeName);
        fireEventListeners(event, d, *legacyListenersVector);
        event->setType(unprefixedTypeName);
    }

    if (log) {
        thisAction->willDeferJoin();
        d->eventListenerMap.setEventAction(event->type(), thisAction);
    }

#ifndef NDEBUG
  out:
#endif
    Editor::countEvent(executionContext(), event);
    countLegacyEvents(legacyTypeName, listenersVector, legacyListenersVector);
    return !event->defaultPrevented();
}

void EventTarget::fireEventListeners(Event* event, EventTargetData* d, EventListenerVector& entry)
{
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();

    // Fire all listeners registered for this event. Don't fire listeners removed
    // during event dispatch. Also, don't fire event listeners added during event
    // dispatch. Conveniently, all new event listeners will be added after or at
    // index |size|, so iterating up to (but not including) |size| naturally excludes
    // new event listeners.

    if (event->type() == EventTypeNames::beforeunload) {
        if (LocalDOMWindow* executingWindow = this->executingWindow()) {
            if (executingWindow->top())
                UseCounter::count(executingWindow->document(), UseCounter::SubFrameBeforeUnloadFired);
            UseCounter::count(executingWindow->document(), UseCounter::DocumentBeforeUnloadFired);
        }
    } else if (event->type() == EventTypeNames::unload) {
        if (LocalDOMWindow* executingWindow = this->executingWindow())
            UseCounter::count(executingWindow->document(), UseCounter::DocumentUnloadFired);
    } else if (event->type() == EventTypeNames::DOMFocusIn || event->type() == EventTypeNames::DOMFocusOut) {
        if (LocalDOMWindow* executingWindow = this->executingWindow())
            UseCounter::count(executingWindow->document(), UseCounter::DOMFocusInOutEvent);
    } else if (event->type() == EventTypeNames::focusin || event->type() == EventTypeNames::focusout) {
        if (LocalDOMWindow* executingWindow = this->executingWindow())
            UseCounter::count(executingWindow->document(), UseCounter::FocusInOutEvent);
    }

    size_t i = 0;
    size_t size = entry.size();
    if (!d->firingEventIterators)
        d->firingEventIterators = adoptPtr(new FiringEventIteratorVector);
    d->firingEventIterators->append(FiringEventIterator(event->type(), i, size));
    for ( ; i < size; ++i) {
        RegisteredEventListener& registeredListener = entry[i];
        if (event->eventPhase() == Event::CAPTURING_PHASE && !registeredListener.useCapture)
            continue;
        if (event->eventPhase() == Event::BUBBLING_PHASE && registeredListener.useCapture)
            continue;

        // If stopImmediatePropagation has been called, we just break out immediately, without
        // handling any more events on this target.
        if (event->immediatePropagationStopped())
            break;

        ExecutionContext* context = executionContext();
        if (!context)
            break;

        InspectorInstrumentationCookie cookie = InspectorInstrumentation::willHandleEvent(this, event, registeredListener.listener.get(), registeredListener.useCapture);
        // To match Mozilla, the AT_TARGET phase fires both capturing and bubbling
        // event listeners, even though that violates some versions of the DOM spec.
        registeredListener.listener->handleEvent(context, event);
        InspectorInstrumentation::didHandleEvent(cookie);
    }
    d->firingEventIterators->removeLast();
}

const EventListenerVector& EventTarget::getEventListeners(const AtomicString& eventType)
{
    DEFINE_STATIC_LOCAL(EventListenerVector, emptyVector, ());

    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    if (log && log->hasAction()) {
        eventTargetAccess(log, Operation::READ_MEMORY, this, eventType);
    }

    EventTargetData* d = eventTargetData();
    if (!d)
        return emptyVector;

    EventListenerVector* listenerVector = d->eventListenerMap.find(eventType);
    if (!listenerVector)
        return emptyVector;

    return *listenerVector;
}

Vector<AtomicString> EventTarget::eventTypes()
{
    EventTargetData* d = eventTargetData();
    return d ? d->eventListenerMap.eventTypes() : Vector<AtomicString>();
}

void EventTarget::removeAllEventListeners()
{
    EventTargetData* d = eventTargetData();
    if (!d)
        return;
    d->eventListenerMap.clear();

    // Notify firing events planning to invoke the listener at 'index' that
    // they have one less listener to invoke.
    if (d->firingEventIterators) {
        for (size_t i = 0; i < d->firingEventIterators->size(); ++i) {
            d->firingEventIterators->at(i).iterator = 0;
            d->firingEventIterators->at(i).end = 0;
        }
    }
}

} // namespace blink
