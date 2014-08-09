/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
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

#ifndef EventListenerMap_h
#define EventListenerMap_h

#include "core/events/RegisteredEventListener.h"
// FIXME: #include "platform/heap/Handle.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class EventAction;

typedef Vector<RegisteredEventListener, 1> EventListenerVector;

struct EventListenerMapEntryInit {
    EventListenerMapEntryInit(const WTF::AtomicString &typ, WTF::PassOwnPtr<EventListenerVector> lis,
                              EventAction *act)
        : type(typ), listeners(lis), action(act)
    {}

    WTF::AtomicString type;
    WTF::PassOwnPtr<EventListenerVector> listeners;
    EventAction *action;
};

struct EventListenerMapEntry {
    EventListenerMapEntry() : action(0) {}
    EventListenerMapEntry(const EventListenerMapEntryInit &init)
        : type(init.type), listeners(init.listeners), action(init.action)
    {}

    WTF::AtomicString type;
    WTF::OwnPtr<EventListenerVector> listeners;
    EventAction *action;
private:
    EventListenerMapEntry(const EventListenerMapEntry &);
};

} // end namespace blink

namespace WTF {
    template<>
    struct VectorTraits<blink::EventListenerMapEntry>
    {
        typedef VectorTraits<AtomicString> FirstTraits;
        typedef VectorTraits<OwnPtr<blink::EventListenerVector> > SecondTraits;

        static const bool needsDestruction = FirstTraits::needsDestruction || SecondTraits::needsDestruction;
        static const bool canInitializeWithMemset = FirstTraits::canInitializeWithMemset && SecondTraits::canInitializeWithMemset;
        static const bool canMoveWithMemcpy = FirstTraits::canMoveWithMemcpy && SecondTraits::canMoveWithMemcpy;
        static const bool canCopyWithMemcpy = FirstTraits::canCopyWithMemcpy && SecondTraits::canCopyWithMemcpy;
        static const bool canFillWithMemset = false;
        static const bool canCompareWithMemcmp = FirstTraits::canCompareWithMemcmp && SecondTraits::canCompareWithMemcmp;
        template <typename U = void>
        struct NeedsTracingLazily {
            static const bool value = ShouldBeTraced<FirstTraits>::value || ShouldBeTraced<SecondTraits>::value;
        };
        static const WeakHandlingFlag weakHandlingFlag = NoWeakHandlingInCollections; // We don't support weak handling in vectors.
    };
} // end namespace WTF

namespace blink {

class EventListenerMap {
public:
    EventListenerMap();

    bool isEmpty() const { return m_entries.isEmpty(); }
    bool contains(const AtomicString& eventType) const;
    bool containsCapturing(const AtomicString& eventType) const;

    void clear();
    bool add(const AtomicString& eventType, PassRefPtr<blink::EventListener>, bool useCapture);
    bool remove(const AtomicString& eventType, blink::EventListener*, bool useCapture, size_t& indexOfRemovedListener);
    blink::EventListenerVector* find(const AtomicString& eventType);
    EventAction *getEventAction(const AtomicString &eventType);
    void setEventAction(const AtomicString &eventType, EventAction *act);
    Vector<AtomicString> eventTypes() const;

    void removeFirstEventListenerCreatedFromMarkup(const AtomicString& eventType);
    void copyEventListenersNotCreatedFromMarkupToTarget(EventTarget*);

private:
    friend class EventListenerIterator;

    void assertNoActiveIterators();

    Vector<blink::EventListenerMapEntry, 2> m_entries;

#if ENABLE(ASSERT)
    int m_activeIteratorCount;
#endif
};

class EventListenerIterator {
    WTF_MAKE_NONCOPYABLE(EventListenerIterator);
public:
    EventListenerIterator();
    EventListenerIterator(EventTarget*);
#if ENABLE(ASSERT)
    ~EventListenerIterator();
#endif

    blink::EventListener* nextListener();

private:
    EventListenerMap* m_map;
    unsigned m_entryIndex;
    unsigned m_index;
};

#if !ENABLE(ASSERT)
inline void EventListenerMap::assertNoActiveIterators() { }
#endif

} // namespace blink

#endif // EventListenerMap_h
