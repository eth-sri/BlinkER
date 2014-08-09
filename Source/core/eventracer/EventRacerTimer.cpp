#include "config.h"
#include "EventRacerContext.h"
#include "EventRacerLog.h"
#include "EventRacerTimer.h"

namespace blink {

void EventRacerTimerBase::start(double nextFireInterval, double repeatInterval, const TraceLocation& caller)
{
    TimerBase::start(nextFireInterval, repeatInterval, caller);

    RefPtr<EventRacerLog> log = EventRacerContext::getLog();
    if (m_data) {
        if (!m_data->log)
            m_data->log = log;
        ASSERT(!log || log == m_data->log);
    } else {
        m_data = EventRacerData::create(log);
    }

    log = m_data->log;
    if (log) {
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
        m_data->log.clear();
        m_data->pred.clear();
        m_data->act = 0;
    }
}

void EventRacerTimerBase::fired()
{
    // Not involving an EventRacer context.
    if (!m_data || !m_data->log) {
        didFire();
        return;
    }

    // Once the timer has been fired, it may be deleted, hence do not access the
    // timer object at all. Instead we keep a reference to a separate struct
    // with the EventRacer related fields, in order to extend the life time of
    // that part of the timer at least for the duration of this function.
    RefPtr<EventRacerData> d(m_data);
    bool isRepeating = !!repeatInterval();

    EventAction *action = d->act;
    d->act = 0;

    EventRacerContext ctx(d->log);
    EventActionScope act(action);

    // Join with the event-actions, which started the timer.
    d->pred.join(d->log, action);

    {
        OperationScope op("timer:fired");
        didFire();
    }

    // If it's a repeating timer, fork the next timer event-action as a
    // successor of the current one. FIXME: the question here is whether the log
    // can change during the invocation of |didFire|.
    if (isRepeating) {
        if (action->isReusable()) {
            action->reuse();
            d->act = action;
        } else {
            d->act = d->log->fork(action);
        }
    }
}

PassRefPtr<EventRacerTimerBase::EventRacerData> EventRacerTimerBase::EventRacerData::create(PassRefPtr<EventRacerLog> log) {
    return adoptRef(new EventRacerData(log));
}

EventRacerTimerBase::EventRacerData::EventRacerData(PassRefPtr<EventRacerLog> lg)
    : log(lg), act(0) {
    serial = WTF::atomicIncrement(reinterpret_cast<int *>(&nextSerial));
}

EventRacerTimerBase::EventRacerData::~EventRacerData() {}

unsigned int EventRacerTimerBase::EventRacerData::nextSerial;

} // end namespace
