#include "config.h"
#include "EventRacerTimer.h"
#include "EventRacerLog.h"

namespace WebCore {

void EventRacerTimerBase::start(double nextFireInterval, double repeatInterval, const TraceLocation& caller)
{
    // Detect attempts to start an obsolete timer.
    EventRacerLog *log = EventRacerLog::current();
    if (log && m_data && m_data->logId && m_data->logId != log->getId())
        return;

    TimerBase::start(nextFireInterval, repeatInterval, caller);

    if (log) {
        if (!m_data)
            m_data = EventRacerData::create();
        m_data->logId = log->getId();
        if (!m_data->act)
            m_data->act = log->fork(log->getCurrentAction());
        else
            m_data->pred.deferJoin(log->getCurrentAction());
    }
}

void EventRacerTimerBase::stop()
{
    TimerBase::stop();

    if (m_data) {
        m_data->logId = 0;
        m_data->pred.clear();
        m_data->act = 0;
    }
}

void EventRacerTimerBase::fired()
{
    // Detect stale timer invocations (after the renderer has navigated to
    // another URL) and ignore the timer in that case.
    EventRacerLog *log = EventRacerLog::current();
    if (!log || log->getId() != m_data->logId)
        return;

    // Not involving an EventRacer context.
    if (!m_data || !m_data->logId) {
        didFire();
        return;
    }

    // Once the timer has been fired, it may be deleted, hence do not access the
    // timer object at all. Instead we keep a reference to a separate struct
    // with the EventRacer related fields, in order to extend the life time of
    // that part of the timer at least for the duration of this function.
    RefPtr<EventRacerData> d(m_data);
    bool isRepeating = !!repeatInterval();

    EventAction *act = d->act;
    d->act = 0;
    d->logId = 0;

    EventActionScope scope(act);

    // Join with the event-actions, which started the timer.
    d->pred.join(log, act);

    {
        OperationScope op("timer:fired");
        didFire();
    }

    // If it's a repeating timer, fork the next timer event-action as a
    // successor of the current one. FIXME: the question here is whether the log
    // can change during the invocation of |didFire|.
    if (isRepeating) {
        d->logId = log->getId();
        if (act->isReusable()) {
            act->reuse();
            d->act = act;
        } else {
            d->act = log->fork(act);
        }
    }
}

EventRacerTimerBase::EventRacerData::EventRacerData() : logId(0), act(0) {}

EventRacerTimerBase::EventRacerData::~EventRacerData() {}

} // end namespace
