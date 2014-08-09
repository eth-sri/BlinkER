#ifndef Operation_h
#define Operation_h

#include <cstddef>

namespace blink {

class Operation {
public:
    enum Type {
        ENTER_SCOPE,
        EXIT_SCOPE,
        READ_MEMORY,
        WRITE_MEMORY,
        TRIGGER_ARC,
        MEMORY_VALUE
    };

    Operation(Type type, size_t loc = 0) : m_type(type), m_location(loc) {}

    Type getType() const { return m_type; }
    size_t getLocation() const { return m_location;}

private:
    Type m_type;

    // Memory location for reads/writes and scope id for scopes.
    // Should be 0 if the location is unused.
    size_t m_location;
};

} // end namespace

#endif // Operation_h
