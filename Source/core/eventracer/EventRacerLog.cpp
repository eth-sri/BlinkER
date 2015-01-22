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
    enum StringTableKind k = SCOPE_STRINGS;
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

void EventRacerLog::logFieldAccess(Operation::Type op, v8::Handle<v8::Object> obj, v8::Handle<v8::String> name) {
    // Check that we in fact have an object.
    if (obj.IsEmpty() || !obj->IsObject())
        return;
    v8::String::Utf8Value str(name);
    WTF::String attr = WTF::String::fromUTF8(*str, str.length());
    v8::Isolate *isolate = v8::Isolate::GetCurrent();
    if (Element *elt = V8Element::toImplWithTypeCheck(isolate, obj)) {
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
            return;
        }
    }
    size_t loc;
    if (obj->IsArray()) {
        if (attr == "length")
            loc = m_strings[VAR_STRINGS].putf("Array[%d]$LEN", obj->GetIdentityHash());
        else
            loc = m_strings[VAR_STRINGS].putf("Array[%d]$[%s]", obj->GetIdentityHash(),
                                              attr.utf8().data());
    } else {
        loc = m_strings[VAR_STRINGS].putf("JSObject[%d].%s", obj->GetIdentityHash(),
                                          attr.utf8().data());
    }
    logOperation(getCurrentAction(), op, loc);
}

void EventRacerLog::logMemoryValue(v8::Handle<v8::Value> v) {
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
        if (Element *elt = V8Element::toImplWithTypeCheck(isolate, v)) {
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
void  EventRacerLog::ER_read(v8::Handle<v8::String> name, v8::Handle<v8::Value> value) {
    v8::String::Utf8Value str(name);
    logOperation(getCurrentAction(), Operation::READ_MEMORY, WTF::String::fromUTF8(*str, str.length()));
    logMemoryValue(value);
}

void EventRacerLog::ER_readProp(v8::Handle<v8::Object> obj, v8::Handle<v8::String> name,
                                v8::Handle<v8::Value> value) {
    logFieldAccess(Operation::READ_MEMORY, obj, name);
    logMemoryValue(value);
}

void EventRacerLog::ER_readArray(v8::Handle<v8::Object> obj) {
    if (!obj->IsArray())
        return;
    logOperation(getCurrentAction(), Operation::READ_MEMORY,
                 m_strings[VAR_STRINGS].putf("Array[%d]$LEN", obj->GetIdentityHash()));
}

void EventRacerLog::ER_write(v8::Handle<v8::String> name, v8::Handle<v8::Value> value) {
    v8::String::Utf8Value str(name);
    logOperation(getCurrentAction(), Operation::WRITE_MEMORY, WTF::String::fromUTF8(*str, str.length()));
    logMemoryValue(value);
}

void EventRacerLog::ER_writeProp(v8::Handle<v8::Object> obj, v8::Handle<v8::String> name,
                                 v8::Handle<v8::Value> value) {
    logFieldAccess(Operation::WRITE_MEMORY, obj, name);
    logMemoryValue(value);
}

void EventRacerLog::ER_writeFunc(v8::Handle<v8::String> name, int id) {
    v8::String::Utf8Value str(name);
    logOperation(getCurrentAction(), Operation::WRITE_MEMORY, WTF::String::fromUTF8(*str, str.length()));
    logOperation(getCurrentAction(), Operation::MEMORY_VALUE,
                 m_strings[VALUE_STRINGS].putf("Function[%d]", id));
}

void EventRacerLog::ER_writePropFunc(v8::Handle<v8::Object> obj, v8::Handle<v8::String> name, int id) {
    logFieldAccess(Operation::WRITE_MEMORY, obj, name);
    logOperation(getCurrentAction(), Operation::MEMORY_VALUE,
                 m_strings[VALUE_STRINGS].putf("Function[%d]", id));
}

void EventRacerLog::ER_writeArray(v8::Handle<v8::Object> obj) {
    if (!obj->IsArray())
        return;
    logOperation(getCurrentAction(), Operation::WRITE_MEMORY,
                 m_strings[VAR_STRINGS].putf("Array[%d]$LEN", obj->GetIdentityHash()));
}

void EventRacerLog::ER_enterFunction(v8::Handle<v8::String> name, int scriptId, int fnId,
                                     int start_line, int end_line) {
    // If there's no script or function id, just use the |name| for the scope.
    if (scriptId == -1 && fnId == -1) {
        v8::String::Utf8Value str(name);
        logOperation(getCurrentAction(), Operation::ENTER_SCOPE, WTF::String::fromUTF8(*str, str.length()));
    } else {
        // Find the script source code and url.
        Script scr = {0,};
        findScript(scriptId, scr);
        logOperation(getCurrentAction(),
                     Operation::ENTER_SCOPE,
                     m_strings[SCOPE_STRINGS].putf("Call (fn=%d #%d) line %d-%d %s [[function:%p]]",
                                                   fnId,
                                                   scr.srcId,
                                                   start_line + 1,
                                                   end_line + 1,
                                                   m_strings[VALUE_STRINGS].peek(scr.urlId),
                                                   0xbadc0de));
    }
}

void EventRacerLog::ER_exitFunction() {
    logOperation(getCurrentAction(), Operation::EXIT_SCOPE, 0, true);
}

void EventRacerLog::ER_delete(v8::Handle<v8::String> name) {
    v8::String::Utf8Value str(name);
    logOperation(getCurrentAction(), Operation::WRITE_MEMORY, WTF::String::fromUTF8(*str, str.length()));
    logOperation(getCurrentAction(), Operation::MEMORY_VALUE, "undefined");
}

void EventRacerLog::ER_deleteProp(v8::Handle<v8::Object> obj, v8::Handle<v8::String> name) {
    logFieldAccess(Operation::WRITE_MEMORY, obj, name);
    logOperation(getCurrentAction(), Operation::MEMORY_VALUE, "undefined");
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

enum ET_OP_codes {
    ER_OP_read          = 1,
    ER_OP_readProp      =  2,
    ER_OP_readArray     =  3,
    ER_OP_write         =  4,
    ER_OP_writeProp     =  5,
    ER_OP_writeFunc     =  6,
    ER_OP_writePropFunc =  7,
    ER_OP_writeArray    =  8,
    ER_OP_enterFunc     =  9,
    ER_OP_exitFunc      = 10,
    ER_OP_delete        = 11,
    ER_OP_deleteProp    = 12,
};

void EventRacerLog::fetch(v8::Handle<v8::Context> ctx) {
    v8::Isolate *isolate = ctx->GetIsolate();

    v8::Local<v8::Object> global = ctx->Global();
    ASSERT(!global.IsEmpty());

#define ER_ARRAYS(V)                            \
    V(ER_op)                                    \
    V(ER_obj)                                   \
    V(ER_name)                                  \
    V(ER_value)                                 \
    V(ER_scriptId)                              \
    V(ER_funcId)                                \
    V(ER_startLine)                             \
    V(ER_endLine)
#define V(name)                                                         \
    v8::Local<v8::Array> name = global->Get(v8String(isolate, #name)).As<v8::Array>(); \
    ASSERT(!name.IsEmpty());
    ER_ARRAYS(V)
#undef V

    uint32_t obj_idx = 0, name_idx = 0, value_idx = 0, script_idx = 0, func_idx = 0, line_idx = 0;
    uint32_t op_count = ER_op->Length();
    for (uint32_t i = 0; i < op_count; ++i) {
        v8::Local<v8::Value> code = ER_op->Get(i);
        v8::Local<v8::Object> obj;
        v8::Local<v8::String> name;
        v8::Local<v8::Value> value;
        int32 script_id, fn_id, start_line, end_line;
        switch (code->Uint32Value()) {
        case ER_OP_read:
            name = ER_name->Get(name_idx++).As<v8::String>();
            value = ER_value->Get(value_idx++);
            ER_read(name, value);
            break;
        case ER_OP_readProp:
            obj = ER_obj->Get(obj_idx++).As<v8::Object>();
            name = ER_name->Get(name_idx++).As<v8::String>();
            value = ER_value->Get(value_idx++);
            ER_readProp(obj, name, value);
            break;
        case ER_OP_readArray:
            obj = ER_obj->Get(obj_idx++).As<v8::Object>();
            ER_readArray(obj);
            break;
        case ER_OP_write:
            name = ER_name->Get(name_idx++).As<v8::String>();
            value = ER_value->Get(value_idx++);
            ER_write(name, value);
            break;
        case ER_OP_writeProp:
            obj = ER_obj->Get(obj_idx++).As<v8::Object>();
            name = ER_name->Get(name_idx++).As<v8::String>();
            value = ER_value->Get(value_idx++);
            ER_writeProp(obj, name, value);
            break;
        case ER_OP_writeFunc:
            name = ER_name->Get(name_idx++).As<v8::String>();
            fn_id = ER_funcId->Get(func_idx++)->Int32Value();
            ER_writeFunc(name, fn_id);
            break;
        case ER_OP_writePropFunc:
            obj = ER_obj->Get(obj_idx++).As<v8::Object>();
            name = ER_name->Get(name_idx++).As<v8::String>();
            fn_id = ER_funcId->Get(func_idx++)->Int32Value();
            ER_writePropFunc(obj, name, fn_id);
            break;
        case ER_OP_writeArray:
            obj = ER_obj->Get(obj_idx++).As<v8::Object>();
            ER_writeArray(obj);
            break;
        case ER_OP_enterFunc:
            name = ER_name->Get(name_idx++).As<v8::String>();
            script_id = ER_scriptId->Get(script_idx++)->Int32Value();
            fn_id = ER_funcId->Get(func_idx++)->Int32Value();
            start_line = ER_startLine->Get(line_idx)->Int32Value();
            end_line = ER_endLine->Get(line_idx++)->Int32Value();
            ER_enterFunction(name, script_id, fn_id, start_line, end_line);
            break;
        case ER_OP_exitFunc:
            ER_exitFunction();
            break;
        case ER_OP_delete:
            name = ER_name->Get(name_idx++).As<v8::String>();
            ER_delete(name);
            break;
        case ER_OP_deleteProp:
            obj = ER_obj->Get(obj_idx++).As<v8::Object>();
            name = ER_name->Get(name_idx++).As<v8::String>();
            ER_deleteProp(obj, name);
            break;
        default:
            WTF_LOG_ERROR("Invalid ER operation code %d\n", code->Uint32Value());
            return;
        }
    }
}

} // end namespace blink
