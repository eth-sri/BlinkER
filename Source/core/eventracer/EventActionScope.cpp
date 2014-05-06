#include "config.h"
#include "EventActionScope.h"
#include "EventRacerLog.h"
#include "core/dom/Document.h"
#include "wtf/Threading.h"

namespace WebCore {

EventActionScope::EventActionScope(Document *doc, EventAction::Type type)
    : m_doc(doc), m_log(doc->getEventRacerLog()) {
    m_action = m_log->beginEventAction(0, type);
    m_link = current();
    current() = this;
}

EventActionScope::EventActionScope(Document *doc, unsigned int id)
    : m_doc(doc), m_log(doc->getEventRacerLog()) {
    m_action = m_log->beginEventAction(id);
    m_link = current();
    current() = this;
}

EventActionScope::~EventActionScope() {
    if (m_link == NULL) {
        m_log->endEventAction(m_action);
        m_log->flush(m_doc, m_action);
    }
    current() = m_link;
}

unsigned int EventActionScope::doFork(EventAction::Type type) {
    return m_log->fork(type);
}

void EventActionScope::doJoin(unsigned int id) {
    m_log->join(id);
}

EventActionScope *&EventActionScope::current() {
    static WTF::ThreadSpecific<EventActionScope *> *scope = new ThreadSpecific<EventActionScope *>;
    return **scope;
}

unsigned int EventActionScope::fork(EventAction::Type type) {
    EventActionScope *scope = current();
    if (scope == NULL)
        return 0;
    return scope->doFork();
}

void EventActionScope::join(unsigned int id) {
    EventActionScope *scope = current();
    if (scope != NULL)
        scope->doJoin(id);
}

} // end namespace
