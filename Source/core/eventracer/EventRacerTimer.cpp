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
            m_data->pred.append(m_data->ctx->getAction());
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
    if (!m_data || !m_data->ctx) {
        didFire();
        return;
    }

    RefPtr<EventRacerData> d(m_data);

    RefPtr<EventRacerContext> ctx = d->ctx;
    d->ctx.clear();

    EventAction *act = d->act;
    d->act = 0;

    EventRacerScope scope(ctx);
    ctx->push(act);
    RefPtr<EventRacerLog> log = ctx->getLog();
    if (!d->pred.isEmpty()) {
        for (size_t i = 0; i < d->pred.size(); ++i)
            log->join(d->pred[i], act);
        d->pred.clear();
    }

    {
        OperationScope op("timer:fired");
        didFire();
    }

    if (ctx) {
        if (repeatInterval()) {
            d->ctx = ctx;
            d->act = log->fork(act);
        }
        ctx->pop();
    }
}

EventRacerTimerBase::EventRacerData::EventRacerData() : act(0) {}

EventRacerTimerBase::EventRacerData::~EventRacerData() {}

} // end namespace
