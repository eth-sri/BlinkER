#ifndef EventRacerLog_h
#define EventRacerLog_h

#include "wtf/RefCounted.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

class Document;

class EventRacerLog : public WTF::RefCounted<EventRacerLog> {
public:
    static WTF::PassRefPtr<EventRacerLog> create();

    void startLog(Document *);

private:
    EventRacerLog();
};

} // namespace WebCore

#endif
