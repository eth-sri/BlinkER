#ifndef EventAction_h
#define EventAction_h

#include "Operation.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include <utility>

namespace WebCore {

class EventAction {
public:
    enum Type {
        UNKNOWN,
        TIMER,
        USER_INTERFACE,
        NETWORK,
        CONTINUATION
    };

    enum State {
        PENDING,
        ACTIVE,
        COMPLETED
    };

    typedef std::pair<unsigned int, unsigned int> Edge;

    static WTF::PassOwnPtr<EventAction> create(unsigned int id, Type type) {
        return WTF::adoptPtr(new EventAction(id, type));
    }

    unsigned int getId() const { return m_id; }

    Type getType() const { return m_type; }

    State getState() const { return m_state; }

    void activate() { ASSERT(m_state != COMPLETED); m_state = ACTIVE; }

    void complete() { m_state = COMPLETED; }

    void addEdge (unsigned int dst) { m_edges.append(dst); }

    typedef WTF::Vector<unsigned int> EdgesType;
    EdgesType       &getEdges()       { return m_edges; }
    const EdgesType &getEdges() const { return m_edges; }

    typedef WTF::Vector<Operation> OpsType;
    OpsType       &getOps()       { return m_ops; }
    const OpsType &getOps() const { return m_ops; }

private:
    EventAction(unsigned int id, Type type)
        : m_id(id), m_type(type), m_state(PENDING)
    {}

    unsigned int m_id;
    Type m_type;
    State m_state; 

    EdgesType m_edges;
    OpsType m_ops;
};

}

#endif // EventAction_h
