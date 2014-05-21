#ifndef EventRacerContext_h
#define EventRacerContext_h

#include "EventAction.h"
#include "core/frame/LocalFrame.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WTF { class String; }

namespace WebCore {

class LocalFrame;
class EventRacerLog;

class EventRacerContext : public WTF::RefCounted<EventRacerContext> {
public:
    static WTF::PassRefPtr<EventRacerContext> create(WTF::PassRefPtr<LocalFrame>);

    WTF::PassRefPtr<LocalFrame> getFrame() const { return m_frame; }
    EventRacerLog *getLog() const { return m_log.get(); }
    EventAction *getAction() const { return m_actions.last(); }
    void push(EventAction *a);//  { m_actions.append(a); }
    void pop();// { m_actions.removeLast(); }
    bool hasAction() const { return !m_actions.isEmpty(); }

    static WTF::RefPtr<EventRacerContext> &current();

private:
    EventRacerContext(WTF::PassRefPtr<LocalFrame>);

    WTF::RefPtr<LocalFrame> m_frame;
    WTF::RefPtr<EventRacerLog> m_log;
    WTF::Vector<EventAction *> m_actions;
};

class EventRacerScope {
public:
    EventRacerScope(WTF::PassRefPtr<LocalFrame>);
    EventRacerScope(WTF::PassRefPtr<EventRacerContext>);
    ~EventRacerScope();

private:
    bool m_first;
};

class EventActionScope {
public:
    EventActionScope(WTF::PassRefPtr<EventRacerContext>, EventAction *);
    ~EventActionScope();

private:
    WTF::RefPtr<EventRacerContext> m_context;
};

class OperationScope {
public:
    OperationScope(const WTF::String &);
    ~OperationScope();
};

} // end namespace WebCore

#endif // EventRacerContext_h
