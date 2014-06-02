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

class EventRacerLogClient;

class EventRacerLog : public WTF::RefCounted<EventRacerLog> {
public:
    ~EventRacerLog();

    static WTF::PassRefPtr<EventRacerLog> create(PassOwnPtr<EventRacerLogClient>);

    // Sends event action data to the host.
    void flush(EventAction *);

    // Creates an event action of the given type.
    EventAction *createEventAction(EventAction::Type type = EventAction::UNKNOWN);

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
    void logOperation(EventAction *act, Operation::Type, size_t = 0);
    void logOperation(EventAction *act, Operation::Type, const WTF::String &);

    // Interns a string.
    size_t intern(const WTF::String &);

private:
    EventRacerLog(WTF::PassOwnPtr<EventRacerLogClient>);

    unsigned int m_nextEventActionId;
    WTF::HashMap<unsigned int, WTF::OwnPtr<EventAction> > m_eventActions;
    WTF::Vector<EventAction::Edge> m_pendingEdges;
    StringSet m_strings;
    size_t m_pendingString;

    OwnPtr<EventRacerLogClient> m_client;
};

} // namespace WebCore

#endif
