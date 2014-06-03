#include "config.h"
#include "EventRacerLog.h"
#include "EventRacerLogClient.h"

namespace WebCore {


WTF::PassRefPtr<EventRacerLog> EventRacerLog::create(WTF::PassOwnPtr<EventRacerLogClient> c) {
    return WTF::adoptRef(new EventRacerLog(c));
}

EventRacerLog::EventRacerLog(PassOwnPtr<EventRacerLogClient> client)
	: m_nextEventActionId(1), m_pendingString(1), m_client(client) {
}

EventRacerLog::~EventRacerLog() {}

// Sends event action data to the host.
void EventRacerLog::flush(EventAction *a) {
    m_client->didCompleteEventAction(*a);
    EventAction::EdgesType::const_iterator i;
    for (i = a->getEdges().begin(); i != a->getEdges().end(); ++i)
        m_pendingEdges.append(std::make_pair(a->getId(), *i));
    if (m_pendingEdges.size()) {
        m_client->didHappenBefore(m_pendingEdges);
        m_pendingEdges.clear();
    }
    if (m_pendingString < m_strings.size()) {
        WTF::Vector<WTF::String> s;
        s.reserveInitialCapacity(m_strings.size() - m_pendingString);
        for (size_t i = m_pendingString; i < m_strings.size(); ++i)
            s.append(m_strings.get(i));
        m_client->didUpdateStringTable(m_pendingString, s);
        m_pendingString = m_strings.size();
    }
}

// Creates an event action of the given type.
EventAction *EventRacerLog::createEventAction(EventAction::Type type) {
    unsigned int id = m_nextEventActionId++;
    WTF::OwnPtr<EventAction> a = EventAction::create(id, type);
    EventAction *ptr = a.get();
    m_eventActions.set(id, a.release());
    return ptr;
}

// Starts a new event action of the given type and identifier. If the given
// id is zero, allocate a new one and assign it to the new event action.
EventAction *EventRacerLog::beginEventAction(unsigned int id, EventAction::Type type) {
    ASSERT(id == 0 || m_eventActions.contains(id));
    EventAction *a;
    if (id == 0)
        a = createEventAction(type);
    else
        a = m_eventActions.get(id);
    a->activate();
    return a;
}

// Marks the end of the given event action.
void EventRacerLog::endEventAction(EventAction *a) {
    a->complete();
}

// Creates a new event action, following in the happens-before relation
// the event action |pred|.
EventAction *EventRacerLog::fork(EventAction *pred, EventAction::Type type) {
    EventAction *new_action = createEventAction(type);
    if (pred) {
        ASSERT(pred->getState() == EventAction::ACTIVE);
        pred->addEdge(new_action->getId());
        logOperation(pred, Operation::TRIGGER_ARC, new_action->getId());
    }
    return new_action;
}

// Creates a new event action, following in the happens-before relation
// the event action with identifier |id|.
EventAction *EventRacerLog::fork(unsigned int id, EventAction::Type type) {
    return fork(m_eventActions.get(id), type);
}

// Creats a happens-before edge from the event-action |pred| to the
// event-action |succ|.
void EventRacerLog::join(EventAction *pred, EventAction *succ) {
    ASSERT(pred && pred->getState() == EventAction::COMPLETED);
    ASSERT(succ && succ->getState() == EventAction::ACTIVE);
    pred->addEdge(succ->getId());
    m_pendingEdges.append(std::make_pair(pred->getId(), succ->getId()));
}

// Creats a happens-before edge from the event-action with the
// identifier |id| to the event-action |succ|.
void EventRacerLog::join(unsigned int id, EventAction *succ) {
    join(m_eventActions.get(id), succ);
}

// Records an operation, performed by the event action |act|.
void EventRacerLog::logOperation(EventAction *act, Operation::Type type, size_t loc) {
    ASSERT(act->getState() == EventAction::ACTIVE);
    act->getOps().append(Operation(type, loc));
}

void EventRacerLog::logOperation(EventAction *act, Operation::Type type,
                                  const WTF::String &loc) {
    ASSERT(act->getState() == EventAction::ACTIVE);
    act->getOps().append(Operation(type, intern(loc)));
}

// Interns a string.
size_t EventRacerLog::intern(const WTF::String &s) {
    return m_strings.put(s);
}

} // end namespace WebCore
