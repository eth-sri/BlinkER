#ifndef EventRacerLog_h
#define EventRacerLog_h

#include "EventAction.h"
#include "StringSet.h"
#include "wtf/HashMap.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include <vector>
#include <utility>

namespace WebCore {

class LocalFrame;

class EventRacerLog : public WTF::RefCounted<EventRacerLog> {
public:
    static WTF::PassRefPtr<EventRacerLog> create();

    // Notifies the host that an new log is created.
    void startLog(LocalFrame *);

    // Sends event action data to the host.
    void flush(LocalFrame *, EventAction *);

    // Creates an event action of the given type.
    EventAction *createEventAction(EventAction::Type type);

    // Starts a new event action of the given type and identifier. If the given
    // id is zero, allocate a new one and assign it to the new event action.
    EventAction *beginEventAction(unsigned int id = 0,
                                  EventAction::Type = EventAction::UNKNOWN);

    // Marks the end of the given event action.
    void endEventAction(EventAction * = NULL);

    // Creates a new event action, following in the happens-before relation
    // the event action |pred|.
    EventAction *fork(EventAction *pred, EventAction::Type = EventAction::UNKNOWN);

    // Creates a new event action, following in the happens-before relation
    // the event action with identifier |id|.
    EventAction *fork(unsigned int id, EventAction::Type = EventAction::UNKNOWN);

    // Creats a happens-before edge from the event-action |pred| to the
    // event-action |succ|.
    void join(EventAction *pred, EventAction *succ);

    // Creats a happens-before edge from the event-action with the
    // identifier |id| to the event-action |succ|.
    void join(unsigned int id, EventAction *succ);

    // Records an operation, performed by the event action |act|.
    void logOperation(EventAction *act, Operation::Type, size_t);
    void logOperation(EventAction *act, Operation::Type, const WTF::String &);

    // Interns a string.
    size_t intern(const WTF::String &);

private:
    EventRacerLog();

    unsigned int m_nextEventActionId;
    WTF::HashMap<unsigned int, WTF::OwnPtr<EventAction> > m_eventActions;
    WTF::Vector<EventAction::Edge> m_pendingEdges;
    StringSet m_strings;
    size_t m_pendingString;
};

} // namespace WebCore

#endif
