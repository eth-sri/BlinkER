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
#include "v8.h"

#include <vector>
#include <utility>

namespace blink {

class DOMWindow;
class EventRacerLogClient;

enum StringTableKind {
    STRING_TABLE_KIND_FIRST = 0,
    VAR_STRINGS =  0,
    SCOPE_STRINGS,
    SOURCE_STRINGS,
    VALUE_STRINGS,
    STRING_TABLE_KIND_COUNT
};

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
    void logOperation(EventAction *act, Operation::Type, size_t = 0, bool =  false);
    void logOperation(EventAction *act, Operation::Type, const WTF::String &, bool = false);

    // Returns the |StringSet| of the given |kind|.
    StringSet &getStrings(enum StringTableKind kind);

    // Checks if there is an active event-action.
    bool hasAction() const { return !!m_currentAction; }

    // Returns the current event-action.
    EventAction *getCurrentAction() const { return m_currentAction; }

    // Pops the old and pushes the new stack frames for each JS read/write
    // operation.
    void pushPopJSFrames();

    // Registers a JS source with its V8 id.
    void registerScript(int line, int column, const char *src, size_t len, const char *url, size_t ulen, int id);

    // Retrieves memory reads/writes accummilate during JS execution.
    void fetch(v8::Handle<v8::Context>);

    // JS instrumentation calls
    void ER_read(v8::Handle<v8::String>, v8::Handle<v8::Value>);
    void ER_readProp(v8::Handle<v8::Object>, v8::Handle<v8::String>, v8::Handle<v8::Value>);
    void ER_readArray(v8::Handle<v8::Object>);
    void ER_write(v8::Handle<v8::String>, v8::Handle<v8::Value>);
    void ER_writeProp(v8::Handle<v8::Object>, v8::Handle<v8::String>, v8::Handle<v8::Value>);
    void ER_writeFunc(v8::Handle<v8::String>, int);
    void ER_writePropFunc(v8::Handle<v8::Object>, v8::Handle<v8::String>, int);
    void ER_writeArray(v8::Handle<v8::Object>);
    void ER_enterFunction(v8::Handle<v8::String>, int, int, int, int);
    void ER_exitFunction();
    void ER_delete(v8::Handle<v8::String>);
    void ER_deleteProp(v8::Handle<v8::Object>, v8::Handle<v8::String>);


private:
    EventRacerLog();

    // Sends event action data to the host.
    void flush(EventAction *);
    void flushAll();
    void flushPendingEdges();
    void flushPendingStrings();

    // Convenience function to log JS object property or DOM element attribute
    // reads or write.
    void logFieldAccess(Operation::Type, v8::Handle<v8::Object>, v8::Handle<v8::String>);

    // Convenience function to format and log a value.
    void logMemoryValue(v8::Handle<v8::Value>);

    unsigned int m_id;
    EventAction *m_currentAction;
    unsigned int m_nextEventActionId;
    typedef WTF::HashMap<unsigned int, WTF::OwnPtr<EventAction> > EventActionsMapType;
    EventActionsMapType m_eventActions;
    WTF::Vector<EventAction::Edge> m_pendingEdges;
    StringSetWithFlush m_strings[STRING_TABLE_KIND_COUNT];

    bool m_needFlushAll;
    OwnPtr<EventRacerLogClient> m_client;

    // Map from V8 script identifiers to ER script identifiers.
    struct Script {
        int line;
        int column;
        size_t srcId;
        size_t urlId;
    };
    WTF::HashMap<int, Script> m_scriptMap;
    bool findScript(int, Script &) const;

    static unsigned int m_nextLogId;
};

} // namespace blink

#endif
