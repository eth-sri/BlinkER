#ifndef EventActionScope_h
#define EventActionScope_h

#include "EventAction.h"
#include "core/frame/LocalFrame.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WTF { class String; }

namespace WebCore {

class LocalFrame;
class EventRacerLog;

class EventActionScope : public WTF::RefCounted<EventActionScope> {
public:
    virtual ~EventActionScope();

    WTF::PassRefPtr<LocalFrame> getFrame() const { return m_frame; }
    EventRacerLog *getLog() const { return m_log.get(); }
    EventAction *getAction() const { return m_action; }

protected:
    EventActionScope(WTF::PassRefPtr<LocalFrame>);

    WTF::RefPtr<LocalFrame> m_frame;
    WTF::RefPtr<EventRacerLog> m_log;
    EventAction *m_action;
};

namespace detail {

class EventActionScopeNested : public EventActionScope {
public:
    EventActionScopeNested(WTF::PassRefPtr<LocalFrame>, EventAction *,
                           const WTF::String &, EventAction::Type);
    EventActionScopeNested(WTF::PassRefPtr<LocalFrame>, EventAction *,
                           const WTF::String &, unsigned int);
    virtual ~EventActionScopeNested();

private:
    bool m_nested;
};

class EventActionScopeFork : public EventActionScope {
public:
    EventActionScopeFork(WTF::PassRefPtr<LocalFrame>, EventAction *,
                         const WTF::String &, EventAction::Type);
    virtual ~EventActionScopeFork();
};

class EventActionScopeJoin : public EventActionScope {
public:
    EventActionScopeJoin(WTF::PassRefPtr<LocalFrame>,
                         const WTF::String &, unsigned int, EventAction::Type);
    virtual ~EventActionScopeJoin();
};

} // end namespace detail

class EventActionHolder {
public:
    EventActionHolder(WTF::PassRefPtr<EventActionScope>);
    ~EventActionHolder();

    static WTF::PassRefPtr<EventActionScope> empty();
    static WTF::PassRefPtr<EventActionScope> begin(LocalFrame *, const WTF::String &,
                                                   EventAction::Type = EventAction::UNKNOWN);
    static WTF::PassRefPtr<EventActionScope> begin(const WTF::String &,
                                                   EventAction::Type = EventAction::UNKNOWN);
    static WTF::PassRefPtr<EventActionScope> begin(LocalFrame *, const WTF::String &,
                                                   unsigned int);
    static WTF::PassRefPtr<EventActionScope> fork(const WTF::String &,
                                                  EventAction::Type = EventAction::UNKNOWN);
    static WTF::PassRefPtr<EventActionScope> join(LocalFrame *, const WTF::String &, unsigned int,
                                                  EventAction::Type = EventAction::UNKNOWN);

    static WTF::RefPtr<EventActionScope> &current();

private:
    WTF::RefPtr<EventActionScope> m_scope;
    WTF::RefPtr<EventActionScope> m_prev;
};

} // end namespace WebCore

#endif // EventActionScope_h
