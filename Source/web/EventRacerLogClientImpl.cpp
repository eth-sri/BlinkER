#include "config.h"
#include "EventRacerLogClientImpl.h"
#include "public/web/WebEventRacer.h"

namespace blink {

PassOwnPtr<EventRacerLogClient> EventRacerLogClientImpl::create(PassOwnPtr<WebEventRacerLogClient> c) {
    return adoptPtr(new EventRacerLogClientImpl(c));
}

EventRacerLogClientImpl::EventRacerLogClientImpl(PassOwnPtr<WebEventRacerLogClient> c)
    : m_client(c) {
}

EventRacerLogClientImpl::~EventRacerLogClientImpl() {}

void EventRacerLogClientImpl::didCompleteEventAction(const EventAction &a) {
    m_client->didCompleteEventAction(WebEventAction(a));
}

void EventRacerLogClientImpl::didHappenBefore(const Vector<EventAction::Edge> &v) {
    m_client->didHappenBefore(WebVector<WebEventActionEdge>(v));
}

void EventRacerLogClientImpl::didUpdateStringTable(size_t kind, const Vector<String> &v) {
    m_client->didUpdateStringTable(kind, WebVector<WebString>(v));
}

} // end namespace blink
