#include "config.h"
#include "EventRacerContext.h"
#include "EventRacerLog.h"
#include "core/frame/LocalFrame.h"
#include "wtf/text/WTFString.h"
#include "wtf/ThreadSpecific.h"

namespace WebCore {

// EventRacerContext -----------------------------------------------------------
EventRacerContext::EventRacerContext(WTF::PassRefPtr<LocalFrame> frame)
    : m_frame(frame), m_log(m_frame->getEventRacerLog()) {
}

WTF::PassRefPtr<EventRacerContext> EventRacerContext::create(WTF::PassRefPtr<LocalFrame> frame) {
    return WTF::adoptRef (new EventRacerContext(frame));
}

void EventRacerContext::push(EventAction *a) {
    m_actions.append(a);
    a->activate();
}

void EventRacerContext::pop() {
    EventAction *a = m_actions.last();
    m_actions.removeLast();
    a->complete();
    m_log->flush(m_frame.get(), a);
}

WTF::RefPtr<EventRacerContext> &EventRacerContext::current() {
    static WTF::ThreadSpecific<WTF::RefPtr<EventRacerContext> > *ctx
        = new ThreadSpecific<WTF::RefPtr<EventRacerContext> >;
    return **ctx;
}

// EventRacerScope -------------------------------------------------------------
EventRacerScope::EventRacerScope(WTF::PassRefPtr<LocalFrame> frame)
    : m_first(!EventRacerContext::current()) {
    if (m_first)
        EventRacerContext::current() = EventRacerContext::create(frame); 
}

EventRacerScope::EventRacerScope(WTF::PassRefPtr<EventRacerContext> ctx)
    : m_first(true) {
    ASSERT(!EventRacerContext::current());
    EventRacerContext::current() = ctx;
}

EventRacerScope::~EventRacerScope() {
    if (m_first)
        EventRacerContext::current().clear();
}

// EventActionScope ------------------------------------------------------------
EventActionScope::EventActionScope(WTF::PassRefPtr<EventRacerContext> ctx, EventAction *act)
    : m_context(ctx) {
    m_context->push(act);
}

EventActionScope::~EventActionScope() {
    m_context->pop();
}

// OperationScope --------------------------------------------------------------
OperationScope::OperationScope(const WTF::String &name) {
    RefPtr<EventRacerContext> ctx = EventRacerContext::current();
    ASSERT(ctx);
    EventAction *act = ctx->getAction();
    ASSERT(act && act->getState() == EventAction::ACTIVE);
    ctx->getLog()->logOperation(act, Operation::ENTER_SCOPE, name);
}

OperationScope::~OperationScope() {
    RefPtr<EventRacerContext> ctx = EventRacerContext::current();
    EventAction *act = ctx->getAction();
    ctx->getLog()->logOperation(act, Operation::EXIT_SCOPE);
}

} // end namespace WebCore
