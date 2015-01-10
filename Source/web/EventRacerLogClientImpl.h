#ifndef EventRacerLogClientImpl_h
#define EventRacerLogClientImpl_h

#include "core/eventracer/EventAction.h"
#include "core/eventracer/EventRacerLogClient.h"
#include "wtf/OwnPtr.h"

namespace blink {

class WebEventRacerLogClient;

class EventRacerLogClientImpl : public EventRacerLogClient {
public:
    static PassOwnPtr<EventRacerLogClient> create(PassOwnPtr<WebEventRacerLogClient>);

    virtual ~EventRacerLogClientImpl();

    virtual void didCompleteEventAction(const EventAction &) override;
    virtual void didHappenBefore(const Vector<EventAction::Edge> &) override;
    virtual void didUpdateStringTable(size_t, const Vector<String> &) override;

private:
    EventRacerLogClientImpl(PassOwnPtr<WebEventRacerLogClient>);
    WTF::OwnPtr<WebEventRacerLogClient> m_client;
};

} // end namespace blink

#endif // EventRacerLogClientImpl_h
