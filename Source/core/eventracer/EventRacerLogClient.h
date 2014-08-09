#ifndef EventRacerLogClient_h
#define EventRacerLogClient_h

#include "EventAction.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <cstddef>

namespace blink {

using WTF::String;
using WTF::Vector;

class EventRacerLogClient {
public:
    virtual ~EventRacerLogClient() {}

    virtual void didCompleteEventAction(const EventAction &) = 0;
    virtual void didHappenBefore(const Vector<EventAction::Edge> &) = 0;
    virtual void didUpdateStringTable(size_t, const Vector<String> &) = 0;
};

} // end namespace blink

#endif // EventRacerLogClient_h
