#ifndef WebEventRacer_h
#define WebEventRacer_h

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
    WebOperation(const WebCore::Operation &op)
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
    WebEventAction(const WebCore::EventAction &a)
    : id(a.getId())
    , type(a.getType())
    , ops(a.getOps().begin(), a.getOps().end())
    {}
#endif
};

typedef std::pair<unsigned int, unsigned int> WebEventActionEdge;

} // end namespace

#endif // WebEventRacer_h

