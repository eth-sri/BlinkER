#ifndef EventRacerLog_h
#define EventRacerLog_h

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8StringResource.h"
#include "EventAction.h"
#include "StringSet.h"
#include "wtf/HashMap.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/ThreadSpecific.h"

#include <vector>
#include <utility>

namespace WebCore {

class EventRacerLogClient;

class EventRacerLog : public RefCounted<EventRacerLog> {
public:
    ~EventRacerLog();

    // Creates a new EventRacer log.
    static PassRefPtr<EventRacerLog> create();

    // Called on comitting a provisional load.
    void connect(PassOwnPtr<EventRacerLogClient>);

    // Returns true if the log is connected to the host.
    bool isConnected() const { return !!m_client; }

    // Returns a unique id of the log (mostly for error checking).
    unsigned int getId() const { return m_id; }

    // Creates an event action of the given type.
    EventAction *createEventAction(EventAction::Type type = EventAction::UNKNOWN);

    // Starts a new event action of the given type and identifier. If the given
    // id is zero, allocate a new one and assign it to the new event action.
    EventAction *beginEventAction(unsigned int id = 0,
                                  EventAction::Type = EventAction::UNKNOWN);

    // Starts a new event action.
    void beginEventAction(EventAction *);

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

    // Formats and interns a string.
    size_t internf(const char *, ...);

    // Checks if there is an active event-action.
    bool hasAction() const { return !!m_currentAction; }

    // Returns the current event-action.
    EventAction *getCurrentAction() const { return m_currentAction; }

    // JS instrumentation calls
    static ScriptValue ER_read(LocalDOMWindow &, const V8StringResource<> &, const ScriptValue &);
    static ScriptValue ER_write(LocalDOMWindow &, const V8StringResource<> &, const ScriptValue &);
    static ScriptValue ER_readProp(LocalDOMWindow &, const ScriptValue &, const V8StringResource<> &,
                                   const ScriptValue &);
    static ScriptValue ER_writeProp(LocalDOMWindow &, const ScriptValue &, const V8StringResource<> &,
                                    const ScriptValue &);
    static ScriptValue ER_readArray(LocalDOMWindow &, const ScriptValue &);
    static ScriptValue ER_writeArray(LocalDOMWindow &, const ScriptValue &);

private:
    EventRacerLog();

    // Sends event action data to the host.
    void flush(EventAction *);
    void flushAll();
    void flushPendingEdges();
    void flushPendingStrings();

    unsigned int m_id;
    EventAction *m_currentAction;
    unsigned int m_nextEventActionId;
    typedef WTF::HashMap<unsigned int, WTF::OwnPtr<EventAction> > EventActionsMapType;
    EventActionsMapType m_eventActions;
    WTF::Vector<EventAction::Edge> m_pendingEdges;
    StringSet m_strings;
    size_t m_pendingString;

    bool m_needFlushAll;
    OwnPtr<EventRacerLogClient> m_client;

    static unsigned int m_nextLogId;
};

} // namespace WebCore

#endif
