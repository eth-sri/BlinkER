#include "config.h"
#include "EventRacerLog.h"
#include "EventRacerLogClient.h"

namespace WebCore {

// EventAction -----------------------------------------------------------------
void EventAction::addEdge(unsigned int dst) {
    m_edges.append(dst);
    if (m_state == COMPLETED) {
        // This must be a deferred join.
        ASSERT(m_deferCount);
        --m_deferCount;
    }
}

// EventRacerLog ---------------------------------------------------------------
WTF::ThreadSpecific<EventRacerLog *> &EventRacerLog::tsLog() {
    static WTF::ThreadSpecific<EventRacerLog *> *log
        = new WTF::ThreadSpecific<EventRacerLog *>;
    return *log;
}

EventRacerLog *EventRacerLog::start(WTF::PassOwnPtr<EventRacerLogClient> c) {
    EventRacerLog *log = new EventRacerLog(c);
    std::swap(log, *tsLog());
    delete log;
    return *tsLog();
}

EventRacerLog *EventRacerLog::current() {
    return *tsLog();
}

unsigned int EventRacerLog::m_nextLogId;

EventRacerLog::EventRacerLog(PassOwnPtr<EventRacerLogClient> client)
	: m_id(++m_nextLogId), m_currentAction(NULL), m_nextEventActionId(1), m_pendingString(1), m_client(client) {
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
    ASSERT(!hasAction());
    m_currentAction = a;
    m_currentAction->activate();
    return m_currentAction;
}

// Starts a new event action.
void EventRacerLog::beginEventAction(EventAction *a) {
    ASSERT(a && !hasAction());
    m_currentAction = a;
    m_currentAction->activate();
}

// Marks the end of the given event action.
void EventRacerLog::endEventAction(EventAction *a) {
    ASSERT(m_currentAction);
    ASSERT(!a || a == m_currentAction);
    if (m_currentAction->getState() == EventAction::ACTIVE) {
        a->complete();
        flush(a);
    }
    m_currentAction = NULL;
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

// EventActionScope ------------------------------------------------------------
EventActionScope::EventActionScope(EventAction *act)
    : m_action(act) {
    EventRacerLog *log = EventRacerLog::current();
    ASSERT(log);
    log->beginEventAction(m_action);
}

EventActionScope::~EventActionScope() {
    EventRacerLog *log = EventRacerLog::current();
    ASSERT(log);
    log->endEventAction(m_action);
}

// OperationScope --------------------------------------------------------------
OperationScope::OperationScope(const WTF::String &name) {
    EventRacerLog *log = EventRacerLog::current();
    ASSERT(log && log->hasAction());
    EventAction *act = log->getCurrentAction();
    ASSERT(act && act->getState() == EventAction::ACTIVE);
    log->logOperation(act, Operation::ENTER_SCOPE, name);
}

OperationScope::~OperationScope() {
    EventRacerLog *log = EventRacerLog::current();
    ASSERT(log && log->hasAction());
    EventAction *act = log->getCurrentAction();
    ASSERT(act && act->getState() == EventAction::ACTIVE);
    log->logOperation(act, Operation::EXIT_SCOPE);
}

} // end namespace WebCore
