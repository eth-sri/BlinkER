#include "config.h"
#include "EventRacerTimer.h"
#include "EventRacerLog.h"

namespace WebCore {

void EventRacerTimerBase::start(double nextFireInterval, double repeatInterval, const TraceLocation& caller)
{
    TimerBase::start(nextFireInterval, repeatInterval, caller);

    EventRacerLog *log = EventRacerLog::current();
    if (log) {
        ASSERT(!m_data || m_data->logId == 0 || m_data->logId == log->getId());
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
    // Not involving an EventRacer context.
    if (!m_data || !m_data->logId) {
        didFire();
        return;
    }

    EventRacerLog *log = EventRacerLog::current();
    ASSERT(log && log->getId() == m_data->logId);

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
    // successor of the current one.
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
