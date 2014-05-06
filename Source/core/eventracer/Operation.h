#ifndef Operation_h
#define Operation_h

namespace WebCore {

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

    Operation(Type type, int loc = -1) : m_type(type), m_location(loc) {}

    Type getType() const { return m_type; }
    int getLocation() const { return m_location;}

private:
    Type m_type;

    // Memory location for reads/writes and scope id for scopes.
    // Should be -1 if the location is unused.
    int m_location;
};

} // end namespace

#endif // Operation_h
