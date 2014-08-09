#ifndef EventRacerTimer_h
#define EventRacerTimer_h

#include "core/eventracer/EventRacerJoinActions.h"
#include "platform/Timer.h"
#include "wtf/RefCounted.h"

namespace blink {

class EventAction;
class EventRacerContext;

class EventRacerTimerBase : public blink::TimerBase {
public:
    virtual void start(double nextFireInterval, double repeatInterval,
                       const blink::TraceLocation&) OVERRIDE;
    virtual void stop() OVERRIDE;

    unsigned int getSerial() const { return m_data ? m_data->serial : 0; }

protected:
    virtual void fired() OVERRIDE;
    virtual void didFire() = 0;

    class EventRacerData : public RefCounted<EventRacerData> {
    public:
        ~EventRacerData();

        static PassRefPtr<EventRacerData> create(PassRefPtr<EventRacerLog>);

        RefPtr<EventRacerLog> log;
        EventRacerJoinActions pred;
        EventAction *act;
        unsigned int serial;
        static unsigned int nextSerial;

    private:
        EventRacerData(PassRefPtr<EventRacerLog>);
    };

    RefPtr<EventRacerData> m_data;
};

template<typename TimerFiredClass>
class EventRacerTimer : public EventRacerTimerBase {
public:
    typedef void (TimerFiredClass::*TimerFiredFunction)(EventRacerTimer*);

    EventRacerTimer(TimerFiredClass* o, TimerFiredFunction f)
        : m_object(o), m_function(f) { }

private:
    virtual void didFire() OVERRIDE { (m_object->*m_function)(this); }

    // FIXME: oilpan: TimerBase should be moved to the heap and m_object should be traced.
    // This raw pointer is safe as long as Timer<X> is held by the X itself (That's the case
    // in the current code base).
    TimerFiredClass* m_object;
    TimerFiredFunction m_function;
};

template<typename TimerFiredClass>
class EventRacerTimerDebug : public EventRacerTimer<TimerFiredClass> {
public:
    typedef void (TimerFiredClass::*TimerFiredFunction)(EventRacerTimer<TimerFiredClass> *);

    EventRacerTimerDebug(TimerFiredClass* o, TimerFiredFunction f)
        : EventRacerTimer<TimerFiredClass>(o, f) {}

    virtual void start(double nextFireInterval, double repeatInterval,
                       const blink::TraceLocation&loc ) {
        EventRacerTimer<TimerFiredClass>::start(nextFireInterval, repeatInterval, loc);
    }
};

} // end namespace

#endif // EventRacerTimer_h
