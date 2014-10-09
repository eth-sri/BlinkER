/*
 * StringSet.h
 *
 *  Created on: May 10, 2012
 *      Author: veselin
 */

#ifndef StringSet_h
#define StringSet_h

#include "wtf/text/WTFString.h"
#include "wtf/Vector.h"

namespace blink {

class EventRacerLogClient;

class StringSet {
public:
    StringSet();

    // Adds a string to the set. Returns the index of the added string. Duplicate
    // strings get identical indices.
    size_t put(const WTF::String &);

    // Adds a UTF-8 encoded string to the set.
    size_t put(const char *, size_t);

    // Formats a string and calls |put|.
    size_t putf(const char *fmt, ...);

    // Returns a copy of the string for an index.
    WTF::String get(size_t index) const;

    // Returns whether the set contains a given string.
    bool contains(const WTF::String &) const;
    bool contains(const WTF::String &, size_t &index) const;

    // Returns the string for an index. The returned pointer is guaranteed
    // to be valid only until the next modification of StringSet.
    const char *peek(size_t index) const;

    // Returns the number of strings in the set.
    size_t size() const { return m_size; }

protected:
    bool findL(const char *, size_t, size_t, size_t &) const;
    size_t hashL(const char *, size_t) const;
    size_t hashZ(const char *, size_t &) const;
    void addHash(size_t, size_t value);
    void addHashNoRehash(size_t, size_t);
    void rehashAll();

    size_t m_size;
    WTF::Vector<char> m_data;
    WTF::Vector<size_t> m_hashes;
};


class StringSetWithFlush : public StringSet {
public:
    StringSetWithFlush();
    void flush(size_t kind, EventRacerLogClient *);
protected:
    size_t m_pending;
};

} // end namespace

#endif // StringSet_h
