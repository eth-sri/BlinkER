#include "config.h"
#include "EventRacerLog.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/FrameLoaderClient.h"

namespace WebCore {

WTF::PassRefPtr<EventRacerLog> EventRacerLog::create()
{
    return WTF::adoptRef(new EventRacerLog());
}

EventRacerLog::EventRacerLog()
	: m_nextEventActionId(1), m_currentEventAction(NULL)
{
}

void EventRacerLog::startLog(Document *doc)
{
    LocalFrame *frame = doc->frame();
    if (frame)
        frame->loader().client()->dispatchDidStartEventRacerLog();
}

// Sends event action data to the host.
void EventRacerLog::flush(Document *doc, EventAction *a) {
    LocalFrame *frame = doc->frame();
    if (frame) {
        frame->loader().client()->dispatchDidCompleteEventAction(*a);
        EventAction::EdgesType::const_iterator i;
        for ( i = a->getEdges().begin(); i != a->getEdges().end(); ++i)
            m_pendingEdges.append(std::make_pair(a->getId(), *i));
        if (m_pendingEdges.size()) {
            frame->loader().client()->dispatchDidHappenBefore(m_pendingEdges);
            m_pendingEdges.clear();
        }
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
    if (m_currentEventAction == NULL) {
        if (id == 0)
            m_currentEventAction = createEventAction(type);
        else
            m_currentEventAction = m_eventActions.get(id);
        m_currentEventAction->activate();
    }
    return m_currentEventAction;
}

// Marks the end of the current event action. A non-null pointer,
// if given, serves for error checking.
void EventRacerLog::endEventAction(EventAction *a) {
    ASSERT(m_currentEventAction);
    ASSERT(a == NULL || a == m_currentEventAction);
    m_currentEventAction->complete();
    m_currentEventAction = NULL;
}

// Creates a new event action, following in the happens-before relation
// the current event action and returns the new id.
unsigned int EventRacerLog::fork(EventAction::Type type) {
    EventAction *new_action = createEventAction(type);
    // ASSERT(m_currentEventAction);
    // ASSERT(m_currentEventAction->getState() == EventAction::ACTIVE);
    if (m_currentEventAction)
        m_currentEventAction->addEdge(new_action->getId());
    return new_action->getId();
}

// Creats a happens-before edge from the event action with the given
// identifier to the current event action.
void EventRacerLog::join(unsigned int id) {
    ASSERT(m_currentEventAction);
    ASSERT(m_currentEventAction->getState() == EventAction::ACTIVE);
    ASSERT(id);
    EventAction *src = m_eventActions.get(id);
    ASSERT (src->getState() == EventAction::COMPLETED);
    src->addEdge(m_currentEventAction->getId());
    m_pendingEdges.append(std::make_pair(src->getId(), m_currentEventAction->getId()));
}

} // end namespace WebCore
