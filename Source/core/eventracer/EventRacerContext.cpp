#include "config.h"
#include "EventRacerContext.h"
#include "EventRacerLog.h"

#include "wtf/ThreadSpecific.h"

namespace blink {

// EventRacerContext ------------------------------------------------------------

EventRacerContext::EventRacerContext(PassRefPtr<EventRacerLog> log)
    : m_log(log) {
    // FIXME: Temporarily allow creation with a NULL log. Intended usage is for
    // the cases, where we must not use the current EventRacer log, yet we don't
    // know the correct one (if any), thus prefer to not record some actions
    // instead of attributing them to the wrong log.
    //  ASSERT(m_log);
    m_prev = current();
    current() = this;
}

EventRacerContext::~EventRacerContext() {
    current() = m_prev;
}

PassRefPtr<EventRacerLog> EventRacerContext::getLog() {
    EventRacerContext *ctx = current();
    if (ctx)
        return ctx->m_log;
    else
        return nullptr;
}

EventRacerContext *&EventRacerContext::current() {
    static WTF::ThreadSpecific<EventRacerContext *> *ctx
        = new ThreadSpecific<EventRacerContext *>;
    return **ctx;
}


// EventActionScope ------------------------------------------------------------
EventActionScope::EventActionScope(EventAction *act)
    : m_action(act) {
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    ASSERT(log);
    log->beginEventAction(m_action);
}

EventActionScope::~EventActionScope() {
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    ASSERT(log);
    log->endEventAction(m_action);
}

// OperationScope --------------------------------------------------------------
OperationScope::OperationScope(const WTF::String &name) {
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    ASSERT(log && log->hasAction());
    EventAction *act = log->getCurrentAction();
    ASSERT(act && act->getState() == EventAction::ACTIVE);
    log->logOperation(act, Operation::ENTER_SCOPE, name);
}

OperationScope::~OperationScope() {
    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    ASSERT(log && log->hasAction());
    EventAction *act = log->getCurrentAction();
    ASSERT(act && act->getState() == EventAction::ACTIVE);
    log->logOperation(act, Operation::EXIT_SCOPE);
}

} // end namespace blink

