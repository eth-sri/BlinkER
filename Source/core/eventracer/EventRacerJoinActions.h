#ifndef EventRacerJoinActions_h
#define EventRacerJoinActions_h

#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class EventAction;
class EventRacerLog;

class EventRacerJoinActions {
public:
    // Addes the event-action to the list of actions to be joined later.
    void deferJoin(EventAction *);

    // Joins all the event-actions in |m_actions| to |act|.
    void join(PassRefPtr<EventRacerLog> log, EventAction *act);

    // Clears/Aborts the pending joins.
    void clear();

    // Checks if there are any actions added to the set.
    bool isEmpty() const { return m_actions.isEmpty(); }

private:
    WTF::Vector<EventAction *> m_actions;
};

}

#endif // EventRacerJoinActions_h
