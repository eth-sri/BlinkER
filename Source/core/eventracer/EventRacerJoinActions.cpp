#include "config.h"
#include "EventRacerJoinActions.h"
#include "EventRacerLog.h"

namespace WebCore {

void EventRacerJoinActions::deferJoin(EventAction *act) {
    act->willDeferJoin();
    m_actions.append(act);
}

void EventRacerJoinActions::join(EventRacerLog *log, EventAction *act) {
    for (size_t i = 0; i < m_actions.size(); ++i)
        log->join(m_actions[i], act);
    m_actions.clear();
}

void EventRacerJoinActions::clear() {
    for (size_t i = 0; i < m_actions.size(); ++i)
        m_actions[i]->abortDeferredJoin();
    m_actions.clear();
}

} // end namespace
