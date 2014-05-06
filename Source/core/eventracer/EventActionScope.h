#ifndef EventActionScope_h
#define EventActionScope_h

#include "EventAction.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSpecific.h"

namespace WebCore {

class Document;
class EventRacerLog;

class EventActionScope {
public:
    EventActionScope(Document *doc, EventAction::Type type = EventAction::UNKNOWN);
    EventActionScope(Document *doc, unsigned int id);
    ~EventActionScope();

    static EventActionScope *&current();
    static unsigned int fork(EventAction::Type = EventAction::UNKNOWN);
    static void join(unsigned int id);

protected:
    unsigned int doFork(EventAction::Type = EventAction::UNKNOWN);
    void doJoin(unsigned int id);

    Document *m_doc;
    WTF::RefPtr<EventRacerLog> m_log;
    EventAction *m_action;
    EventActionScope *m_link;
};

} // end namespace

#endif // EventActiomnScopr_h
