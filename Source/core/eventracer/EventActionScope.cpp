#include "config.h"
#include "EventActionScope.h"
#include "EventRacerLog.h"
#include "core/frame/LocalFrame.h"
#include "wtf/text/WTFString.h"
#include "wtf/ThreadSpecific.h"

namespace WebCore {

// EventActionScope ------------------------------------------------------------
EventActionScope::EventActionScope(WTF::PassRefPtr<LocalFrame> frame)
    : m_frame(frame), m_log(m_frame->getEventRacerLog()), m_action(NULL) {
}

EventActionScope::~EventActionScope() {}

namespace detail {

// EventActionScopeNested ------------------------------------------------------
EventActionScopeNested::EventActionScopeNested(WTF::PassRefPtr<LocalFrame> frame,
                                               EventAction *act,
                                               const WTF::String &name,
                                               EventAction::Type type)
    : EventActionScope(frame), m_nested(false) {
    if (act) {
        m_action = act;
        m_nested = true;
    } else {
        m_action = m_log->beginEventAction(0, type);
    }
    m_log->logOperation(m_action, Operation::ENTER_SCOPE, name);
}

EventActionScopeNested::EventActionScopeNested(WTF::PassRefPtr<LocalFrame> frame,
                                               EventAction *act,
                                               const WTF::String &name,
                                               unsigned int id)
    : EventActionScope(frame), m_nested(false) {
    if (act) {
        m_action = act;
        m_nested = true;
    } else {
        m_action = m_log->beginEventAction(id);
    }
    ASSERT(id == m_action->getId());
    m_log->logOperation(m_action, Operation::ENTER_SCOPE, name);
}

EventActionScopeNested::~EventActionScopeNested() {
    m_log->logOperation(m_action, Operation::EXIT_SCOPE, 0);
    if (!m_nested) {
        m_log->endEventAction(m_action);
        m_log->flush(m_frame.get(), m_action);
    }
}

// EventActionScopeFork --------------------------------------------------------
EventActionScopeFork::EventActionScopeFork(WTF::PassRefPtr<LocalFrame> frame,
                                           EventAction *act,
                                           const WTF::String &name,
                                           EventAction::Type type)
    : EventActionScope(frame) {
    ASSERT(act);
    m_action = m_log->fork(act, type);
    m_log->beginEventAction(m_action->getId());
    m_log->logOperation(m_action, Operation::ENTER_SCOPE, name);
}

EventActionScopeFork::~EventActionScopeFork() {
    m_log->logOperation(m_action, Operation::EXIT_SCOPE, 0);
    m_log->endEventAction(m_action);
    m_log->flush(m_frame.get(), m_action);
}

// EventActionScopeJoin --------------------------------------------------------
EventActionScopeJoin::EventActionScopeJoin(WTF::PassRefPtr<LocalFrame> frame,
                                           const WTF::String &name, unsigned int id,
                                           EventAction::Type type)
    : EventActionScope(frame) {
    m_action = m_log->beginEventAction(0, type);
    m_log->join(id, m_action);
    m_log->logOperation(m_action, Operation::ENTER_SCOPE, name);
}

EventActionScopeJoin::~EventActionScopeJoin() {
    m_log->logOperation(m_action, Operation::EXIT_SCOPE, 0);
    m_log->endEventAction(m_action);
    m_log->flush(m_frame.get(), m_action);
}

} // end namespace detail

WTF::PassRefPtr<EventActionScope> EventActionHolder::empty() {
    return WTF::PassRefPtr<EventActionScope>();
}

WTF::PassRefPtr<EventActionScope> EventActionHolder::begin(
    LocalFrame *frame, const WTF::String &name, EventAction::Type type) {

    RefPtr<LocalFrame> frm(frame);
    EventAction *prev = current() ? current()->getAction() : 0;
    return WTF::adoptRef(new detail::EventActionScopeNested(frm, prev, name, type));
}

WTF::PassRefPtr<EventActionScope> EventActionHolder::begin(
    const WTF::String &name, EventAction::Type type) {

    ASSERT(current());
    RefPtr<LocalFrame> frm(current()->getFrame());
    EventAction *prev = current()->getAction();
    return WTF::adoptRef(new detail::EventActionScopeNested(frm, prev, name, type));
}

WTF::PassRefPtr<EventActionScope> EventActionHolder::begin(
    LocalFrame *frame, const WTF::String &name, unsigned int id) {

    ASSERT(current());
    RefPtr<LocalFrame> frm(frame);
    EventAction *prev = current()->getAction();
    return WTF::adoptRef(new detail::EventActionScopeNested(frm, prev, name, id));
}

WTF::PassRefPtr<EventActionScope> EventActionHolder::fork(
    const WTF::String &name, EventAction::Type type) {

    ASSERT(current());
    RefPtr<LocalFrame> frm(current()->getFrame());
    EventAction *prev = current()->getAction();
    return WTF::adoptRef(new detail::EventActionScopeFork(frm, prev, name, type));
}

WTF::PassRefPtr<EventActionScope> EventActionHolder::join(
    LocalFrame *frame, const WTF::String &name, unsigned int id, EventAction::Type type) {

    RefPtr<LocalFrame> frm(frame);
    return WTF::adoptRef(new detail::EventActionScopeJoin(frm, name, id, type));
}

WTF::RefPtr<EventActionScope> &EventActionHolder::current() {
    static WTF::ThreadSpecific<WTF::RefPtr<EventActionScope> > *scope
        = new ThreadSpecific<WTF::RefPtr<EventActionScope> >;
    return **scope;
}

EventActionHolder::EventActionHolder(PassRefPtr<EventActionScope> scope)
    : m_scope(scope) {

    m_prev = current();
    current() = m_scope;
}

EventActionHolder::~EventActionHolder() {
    current() = m_prev;
}

} // end namespace WebCore
