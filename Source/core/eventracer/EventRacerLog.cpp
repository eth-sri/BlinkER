#include "config.h"
#include "bindings/core/v8/V8Element.h"
#include "EventRacerContext.h"
#include "EventRacerLog.h"
#include "EventRacerLogClient.h"
#include <stdarg.h>
#include <stdio.h>

namespace blink {

// EventAction -----------------------------------------------------------------
void EventAction::addEdge(unsigned int dst) {
    m_edges.append(dst);
}

// EventRacerLog ---------------------------------------------------------------
PassRefPtr<EventRacerLog> EventRacerLog::create() {
    return adoptRef(new EventRacerLog);
}

unsigned int EventRacerLog::m_nextLogId;

EventRacerLog::EventRacerLog()
    : m_id(++m_nextLogId), m_currentAction(NULL), m_nextEventActionId(1),
      m_needFlushAll(true)
{}

EventRacerLog::~EventRacerLog() {}

// Called on comitting a provisional load.
void EventRacerLog::connect(PassOwnPtr<EventRacerLogClient> c) {
    ASSERT(!m_client);
    m_client = c;

    // If there's no current event-action, flush the darta to the host,
    // otherwise let the event-action do this, upon its end.
    if (!m_currentAction)
        flushAll();
}

// Sends event action data to the host.
void EventRacerLog::flush(EventAction *a) {
    if (!m_client)
        return;
    if (m_needFlushAll) {
        flushAll();
    } else {
        m_client->didCompleteEventAction(*a);
        EventAction::EdgesType::const_iterator i;
        for (i = a->getEdges().begin(); i != a->getEdges().end(); ++i)
            m_pendingEdges.append(std::make_pair(a->getId(), *i));
        flushPendingEdges();
        flushPendingStrings();
    }
}

void EventRacerLog::flushAll() {
    m_needFlushAll = false;
    EventActionsMapType::const_iterator i;
    for(i = m_eventActions.begin(); i != m_eventActions.end(); ++i) {
        EventAction *a = i->value.get();
        m_client->didCompleteEventAction(*a);

        EventAction::EdgesType::const_iterator j;
        for (j = a->getEdges().begin(); j != a->getEdges().end(); ++j)
            m_pendingEdges.append(std::make_pair(a->getId(), *j));
    }
    flushPendingEdges();
    flushPendingStrings();
}

void EventRacerLog::flushPendingEdges() {
    if (m_pendingEdges.size()) {
        m_client->didHappenBefore(m_pendingEdges);
        m_pendingEdges.clear();
    }
}

void EventRacerLog::flushPendingStrings() {
    for (int k = STRING_TABLE_KIND_FIRST; k < STRING_TABLE_KIND_COUNT; ++k)
        m_strings[k].flush(k, m_client.get());
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
void EventRacerLog::logOperation(EventAction *act, Operation::Type type, size_t loc, bool ignoreEmpty) {
    ASSERT(act->getState() == EventAction::ACTIVE);
    if (ignoreEmpty && type == Operation::EXIT_SCOPE) {
        EventAction::OpsType &ops = act->getOps();
        if (ops.size() && ops.last().getType() == Operation::ENTER_SCOPE) {
            ops.shrink(ops.size() - 1);
            return;
        }
    }
    act->getOps().append(Operation(type, loc));
}

void EventRacerLog::logOperation(EventAction *act, Operation::Type type,
                                 const WTF::String &loc, bool ignoreEmpty) {
    ASSERT(act->getState() == EventAction::ACTIVE);
    enum StringTableKind k;
    switch(type) {
    case Operation::ENTER_SCOPE:
        k = SCOPE_STRINGS;
        break;
    case Operation::READ_MEMORY:
    case Operation::WRITE_MEMORY:
        k = VAR_STRINGS;
        break;
    case Operation::MEMORY_VALUE:
        k = VALUE_STRINGS;
        break;
    default:
    case Operation::EXIT_SCOPE:
    case Operation::TRIGGER_ARC:
        ASSERT_NOT_REACHED();
    }
    logOperation(act, type, m_strings[k].put(loc), ignoreEmpty);
}

// Returns the |StringSet| of the given |kind|.
StringSet &EventRacerLog::getStrings(enum StringTableKind kind) {
    ASSERT(kind < STRING_TABLE_KIND_COUNT);
    return m_strings[kind];
}

void EventRacerLog::logFieldAccess(Operation::Type op, const ScriptValue &obj,
                                   const V8StringResource<> &name,
                                   const ScriptValue *val, int fnid) {
    // Check that we in fact have an object.
    v8::Handle<v8::Value> v = obj.v8Value();
    if (!v->IsObject())
        return;
    v8::Local<v8::Object> object = v->ToObject();
    WTF::String attr(name);
    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    if (Element *elt = V8Element::toNativeWithTypeCheck(isolate, object)) {
        // If we've got an DOM Element wrapper, use "DOMNode" kind of memory
        // location in order to be able to detect potentiall conflicts with
        // |getAttribute| and |setAttribute|. Do this only for |id| and |class|
        // attributes, other property read/writes are logged as usual.
        if (attr == "className")
            attr = "class";
        if (attr == "id" || attr == "class") {
            logOperation(getCurrentAction(), op,
                         m_strings[VAR_STRINGS].putf("DOMNode[0x%x].%s", elt->getSerial(),
                                                     attr.utf8().data()));
            if (val)
                logMemoryValue(*val, fnid);
            return;
        }
    }

    logOperation(getCurrentAction(), op,
                 m_strings[VAR_STRINGS].putf("JSObject[%d].%s", object->GetIdentityHash(),
                                             attr.utf8().data()));
    if (val)
        logMemoryValue(*val, fnid);
}

void EventRacerLog::logMemoryValue(const ScriptValue &val, int fnid) {
    if (fnid > 0) {
        logOperation(getCurrentAction(), Operation::MEMORY_VALUE,
                     m_strings[VALUE_STRINGS].putf("Function[%d]", fnid));
        return;
    }
    v8::Handle<v8::Value> v = val.v8Value();
    if (v->IsUndefined())
        logOperation(getCurrentAction(), Operation::MEMORY_VALUE, "undefined");
    else if (v->IsNull())
        logOperation(getCurrentAction(), Operation::MEMORY_VALUE, "NULL");
    else if (v->IsTrue())
        logOperation(getCurrentAction(), Operation::MEMORY_VALUE, "true");
    else if (v->IsFalse())
        logOperation(getCurrentAction(), Operation::MEMORY_VALUE, "false");
    else if (v->IsString()) {
        v8::String::Utf8Value str(v);
        if (str.length()) {
            logOperation(getCurrentAction(), Operation::MEMORY_VALUE,
                         m_strings[VALUE_STRINGS].put(*str, str.length()));
        }
    } else if (v->IsStringObject()) {
        v8::String::Utf8Value str(v);
        if (str.length()) {
            logOperation(getCurrentAction(), Operation::MEMORY_VALUE,
                         m_strings[VALUE_STRINGS].put(*str, str.length()));
        }
    } else if (v->IsObject()) {
        v8::Local<v8::Object> object = v->ToObject();
        v8::Isolate *isolate = v8::Isolate::GetCurrent();
        if (Element *elt = V8Element::toNativeWithTypeCheck(isolate, object)) {
            logOperation(getCurrentAction(), Operation::MEMORY_VALUE,
                         m_strings[VALUE_STRINGS].putf("DOMNode[0x%x]", elt->getSerial()));
        } else {
            logOperation(getCurrentAction(), Operation::MEMORY_VALUE,
                         m_strings[VALUE_STRINGS].putf("JSObject[%d]", object->GetIdentityHash()));
        }
    } else if (v->IsBoolean())
        logOperation(getCurrentAction(), Operation::MEMORY_VALUE,
                     m_strings[VALUE_STRINGS].put(v->BooleanValue() ? "true" : "false"));
    else if (v->IsBooleanObject()) {
        logOperation(getCurrentAction(), Operation::MEMORY_VALUE,
                     m_strings[VALUE_STRINGS].put(v->BooleanValue() ? "true" : "false"));
    } else if (v->IsNumber())
        logOperation(getCurrentAction(), Operation::MEMORY_VALUE,
                     m_strings[VALUE_STRINGS].putf("%f", v->NumberValue()));
    else if (v->IsNumberObject())
        logOperation(getCurrentAction(), Operation::MEMORY_VALUE,
                     m_strings[VALUE_STRINGS].putf("%f", v->NumberValue()));
    else if (v->IsInt32())
        logOperation(getCurrentAction(), Operation::MEMORY_VALUE,
                     m_strings[VALUE_STRINGS].putf("%d", v->Int32Value()));
    else if (v->IsUint32())
        logOperation(getCurrentAction(), Operation::MEMORY_VALUE,
                     m_strings[VALUE_STRINGS].putf("%d", v->Uint32Value()));
}

// JS instrumentation calls
ScriptValue EventRacerLog::ER_read(LocalDOMWindow &, const V8StringResource<> &name,
                                   const ScriptValue &val) {
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    if (log && log->hasAction()) {
        log->logOperation(log->getCurrentAction(), Operation::READ_MEMORY, WTF::String(name));
        log->logMemoryValue(val);
    }
    return val;
}

ScriptValue EventRacerLog::ER_write(LocalDOMWindow &, const V8StringResource<> &name,
                                    const ScriptValue &val) {
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    if (log && log->hasAction()) {
        log->logOperation(log->getCurrentAction(), Operation::WRITE_MEMORY, WTF::String(name));
        log->logMemoryValue(val);
    }
    return val;
}

ScriptValue EventRacerLog::ER_writeFunc(LocalDOMWindow &, const V8StringResource<> &name,
                                        const ScriptValue &val, int id) {
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    if (log && log->hasAction()) {
        log->logOperation(log->getCurrentAction(), Operation::WRITE_MEMORY, WTF::String(name));
        log->logMemoryValue(val, id);
    }
    return val;
}

ScriptValue EventRacerLog::ER_readProp(LocalDOMWindow &, const ScriptValue &obj,
                                       const V8StringResource<> &name, const ScriptValue &val) {
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    if (log && log->hasAction())
        log->logFieldAccess(Operation::READ_MEMORY, obj, name, &val);
    return val;
}

ScriptValue EventRacerLog::ER_writeProp(LocalDOMWindow &, const ScriptValue &obj,
                                        const V8StringResource<> &name, const ScriptValue &val) {
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    if (log && log->hasAction())
        log->logFieldAccess(Operation::WRITE_MEMORY, obj, name, &val);
    return val;
}

ScriptValue EventRacerLog::ER_writePropFunc(LocalDOMWindow &, const ScriptValue &obj,
                                            const V8StringResource<> &name, const ScriptValue &val,
                                            int id) {
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    if (log && log->hasAction())
        log->logFieldAccess(Operation::WRITE_MEMORY, obj, name, &val, id);
    return val;
}

void EventRacerLog::ER_delete(LocalDOMWindow &, const V8StringResource <>&name) {
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    if (log && log->hasAction()) {
        log->logOperation(log->getCurrentAction(), Operation::WRITE_MEMORY, WTF::String(name));
        log->logOperation(log->getCurrentAction(), Operation::MEMORY_VALUE, "undefined");
    }
}

void EventRacerLog::ER_deleteProp(LocalDOMWindow &, const ScriptValue &obj,
                                  const V8StringResource<> &name) {
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    if (log && log->hasAction())
        log->logFieldAccess(Operation::WRITE_MEMORY, obj, name, NULL);
}

void EventRacerLog::ER_enterFunction(LocalDOMWindow &, const V8StringResource<> &, int scriptId, int fnId) {
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    if (log && log->hasAction()) {
        // Find the script source code and url.
        Script scr = {0,};
        log->findScript(scriptId, scr);

        // Get the line number from the stack trace.
        int line = 0;
        v8::Local<v8::StackTrace> trace = v8::StackTrace::CurrentStackTrace(
            v8::Isolate::GetCurrent(), 2,
            v8::StackTrace::kLineNumber);
        if (trace->GetFrameCount() > 1)
            line = trace->GetFrame(1)->GetLineNumber() - scr.line;
        log->logOperation(log->getCurrentAction(), Operation::ENTER_SCOPE,
                          log->m_strings[SCOPE_STRINGS].putf("Call (fn=%d #%d) line %d-%d %s [[function:%p]]",
                                                             fnId,
                                                             scr.srcId,
                                                             line,
                                                             0,
                                                             log->m_strings[VALUE_STRINGS].peek(scr.urlId),
                                                             0xbadc0de));
    }
}

ScriptValue EventRacerLog::ER_exitFunction(LocalDOMWindow &, const ScriptValue &val) {
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    if (log && log->hasAction())
        log->logOperation(log->getCurrentAction(), Operation::EXIT_SCOPE, 0, true);

    return val;
}

ScriptValue EventRacerLog::ER_readArray(LocalDOMWindow &, const ScriptValue &arr) {
    return arr;
}

ScriptValue EventRacerLog::ER_writeArray(LocalDOMWindow &, const ScriptValue &arr) {
    return arr;
}

bool EventRacerLog::findScript(int id, Script &out) const {
    WTF::HashMap<int, Script>::const_iterator p = m_scriptMap.find(id);
    if (p == m_scriptMap.end())
        return false;
    else {
        out = p->value;
        return true;
    }
}

// Registers a JS source with its V8 id.
void EventRacerLog::registerScript(int line, int column, const char *src, size_t srcLen, const char *url, size_t urlLen, int id) {
    if (!m_scriptMap.contains(id)) {
        Script e = {line,
                    column,
                    m_strings[SOURCE_STRINGS].put(src, srcLen),
                    m_strings[VALUE_STRINGS].put(url, urlLen)};
        m_scriptMap.add(id, e);
    }
}

} // end namespace blink
