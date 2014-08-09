#ifndef EventRacerContext_h
#define EventRacerContext_h

#include "EventAction.h"
#include "wtf/RefPtr.h"

namespace WTF {
class String;
}

namespace blink {

class EventRacerLog;

class EventRacerContext {
public:
    EventRacerContext(PassRefPtr<EventRacerLog>);
    ~EventRacerContext();
    static PassRefPtr<EventRacerLog> getLog();

private:
    static EventRacerContext *&current();

    RefPtr<EventRacerLog> m_log;
    EventRacerContext *m_prev;
};

class EventActionScope {
public:
    EventActionScope(EventAction *);
    ~EventActionScope();
private:
    EventAction *m_action;
};

class OperationScope {
public:
    OperationScope(PassRefPtr<EventRacerLog>, const WTF::String &);
    OperationScope(const WTF::String &);
    ~OperationScope();
private:
};

} // end namespace blink

#endif // EventRacerContext_h
