#include "config.h"
#include "EventRacerTimer.h"
#include "EventRacerContext.h"
#include "EventRacerLog.h"

namespace WebCore {

void EventRacerTimerBase::start(double nextFireInterval, double repeatInterval, const TraceLocation& caller)
{
    TimerBase::start(nextFireInterval, repeatInterval, caller);

    if (EventRacerContext::current()) {
        if (!m_data)
            m_data = EventRacerData::create();
        RefPtr<EventRacerLog> log = EventRacerContext::current()->getLog();
        ASSERT(!m_data->ctx || m_data->ctx->getLog() == log);
        m_data->ctx = EventRacerContext::current();
        if (!m_data->act)
            m_data->act = log->fork(m_data->ctx->getAction());
        else
            m_data->pred.deferJoin(m_data->ctx->getAction());
    }
}

void EventRacerTimerBase::stop()
{
    TimerBase::stop();

    if (m_data) {
        m_data->ctx.clear();
        m_data->pred.clear();
        m_data->act = 0;
    }
}

void EventRacerTimerBase::fired()
{
    // Not involving an EventRacer context.
    if (!m_data || !m_data->ctx) {
        didFire();
        return;
    }

    // Once the timer has been fired, it may be deleted, hence do not access the
    // timer object at all. Instead we keep a reference to a separate struct
    // with the EventRacer related fields, in order to extend the life time of
    // that part of the timer at least for the duration of this function.
    RefPtr<EventRacerData> d(m_data);

    RefPtr<EventRacerContext> ctx = d->ctx;
    d->ctx.clear();

    EventAction *act = d->act;
    d->act = 0;

    EventRacerScope scope(ctx);
    ctx->push(act);

    // Join with the event-actions, which started the timer.
    RefPtr<EventRacerLog> log = ctx->getLog();
    d->pred.join(log, act);

    {
        OperationScope op("timer:fired");
        didFire();
    }

    // If it's a repeating timer, fork the next timer event-action as a
    // successor of the current one.
    if (repeatInterval()) {
        d->ctx = ctx;
        d->act = log->fork(act);
    }
    ctx->pop();
}

EventRacerTimerBase::EventRacerData::EventRacerData() : act(0) {}

EventRacerTimerBase::EventRacerData::~EventRacerData() {}

} // end namespace
