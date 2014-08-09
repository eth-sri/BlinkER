#ifndef WebEventRacer_h
#define WebEventRacer_h

#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include <vector>
#include <utility>

#if BLINK_IMPLEMENTATION
#include "core/eventracer/EventAction.h"
#endif

namespace blink {

struct WebOperation {
    unsigned int type; // WebCore::Operation::Type
    size_t location;

#if BLINK_IMPLEMENTATION
    WebOperation(const Operation &op)
    : type(op.getType())
    , location(op.getLocation())
    {}
#endif
};

struct WebEventAction {
    unsigned int id;
    unsigned int type; // WebCore::EventAction::Type
    std::vector<WebOperation> ops;

#if BLINK_IMPLEMENTATION
    WebEventAction(const EventAction &a)
    : id(a.getId())
    , type(a.getType())
    , ops(a.getOps().begin(), a.getOps().end())
    {}
#endif
};

typedef std::pair<unsigned int, unsigned int> WebEventActionEdge;

class WebEventRacerLogClient {
public:
    virtual ~WebEventRacerLogClient() {}

    virtual void didCompleteEventAction(const WebEventAction &) = 0;
    virtual void didHappenBefore(const WebVector<WebEventActionEdge> &) = 0;
    virtual void didUpdateStringTable(size_t, const WebVector<WebString> &) = 0;
};

} // end namespace

#endif // WebEventRacer_h
