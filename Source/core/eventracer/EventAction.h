#ifndef EventAction_h
#define EventAction_h

#include "Operation.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include <utility>

namespace blink {

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

    void willDeferJoin() { ++m_deferCount; }

    void abortDeferredJoin() { --m_deferCount; }

    void addEdge(unsigned int dst);

    bool isLeaf() const { return m_deferCount == 0 && m_edges.size() == 0; }

    bool isReusable() const {
        return isLeaf()
               && (m_ops.size() == 0
                   || (m_ops.size() == 2 && m_ops[1].getType() == Operation::EXIT_SCOPE));
    }

    void reuse() {
        ASSERT(m_state == ACTIVE);
        m_state = PENDING;
        m_ops.clear();
    }

    typedef WTF::Vector<unsigned int> EdgesType;
    EdgesType       &getEdges()       { return m_edges; }
    const EdgesType &getEdges() const { return m_edges; }

    typedef WTF::Vector<Operation> OpsType;
    OpsType       &getOps()       { return m_ops; }
    const OpsType &getOps() const { return m_ops; }

private:
    EventAction(unsigned int id, Type type)
        : m_id(id), m_type(type), m_state(PENDING), m_deferCount(0)
    {}

    unsigned int m_id;
    Type m_type;
    State m_state; 

    unsigned int m_deferCount;
    EdgesType m_edges;
    OpsType m_ops;
};

}

#endif // EventAction_h
