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

class Document;

class EventRacerLog : public WTF::RefCounted<EventRacerLog> {
public:
    static WTF::PassRefPtr<EventRacerLog> create();

    // Notifies the host that an new log is created.
    void startLog(Document *);

    // Sends event action data to the host.
    void flush(Document *, EventAction *);

    // Returns true if there is an active event action.
    bool isInsideEventAction() const { return m_currentEventAction != NULL; }

    // Returns the identifier of the current event action.
    EventAction *getCurrentEventAction() const { return m_currentEventAction; }

    // Creates an event action of the given type.
    EventAction *createEventAction(EventAction::Type type);

    // Starts a new event action of the given type and identifier. If the given
    // id is zero, allocate a new one and assign it to the new event action.
    EventAction *beginEventAction(unsigned int id = 0,
                                  EventAction::Type = EventAction::UNKNOWN);

    // Marks the end of the current event action. A non-null pointer,
    // if given, serves for error checking.
    void endEventAction(EventAction * = NULL);

    // Creates a new event action, following in the happens-before relation
    // the current event.
    unsigned int fork(EventAction::Type = EventAction::UNKNOWN);

    // Creats a happens-before edge from the event action with the given
    // identifier to the current event action.
    void join(unsigned int id);

    // Records an operation, performed by the current event action.
    void logOperation(Operation::Type, size_t);
    void logOperation(Operation::Type, const WTF::String &);

    // Interns a string.
    size_t intern(const WTF::String &);

private:
    EventRacerLog();

    unsigned int m_nextEventActionId;
    EventAction *m_currentEventAction;
    WTF::HashMap<unsigned int, WTF::OwnPtr<EventAction> > m_eventActions;
    WTF::Vector<EventAction::Edge> m_pendingEdges;
    StringSet m_strings;
    size_t m_pendingString;
};

} // namespace WebCore

#endif
