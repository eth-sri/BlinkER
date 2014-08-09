/*
 * StringSet.cpp
 *
 *  Created on: May 10, 2012
 *      Author: veselin
 */

#include "config.h"
#include "StringSet.h"
#include <string.h>

namespace blink {

StringSet::StringSet() {
    m_hashes.resize(33);
    for (size_t i = 0; i < m_hashes.size(); ++i)
        m_hashes[i] = 0;
    m_offsets.append(0);
}

// Adds a string to the set. Returns the index of the added string. Duplicate
// strings get identical indices.
size_t StringSet::put(const WTF::String &s) {
    return addL(s.characters8(), s.length());
}

// Returns a copy of the string for an index.
WTF::String StringSet::get(size_t index) const {
    return WTF::String(peek(index));
}

// Returns whether the set contains a given string.
bool StringSet::contains(const WTF::String &s) const {
    size_t index;
    return contains(s, index);
}

// Returns whether the set contains a given string.
bool StringSet::contains(const WTF::String &s, size_t &index) const {
    size_t hash = hashL(s.characters8(), s.length());
    return findL(s.characters8(), s.length(), hash, index);
}

// Returns the string for an index. The returned pointer is guaranteed
// to be valid only until the next modification of StringSet.
const LChar *StringSet::peek(size_t index) const {
    ASSERT(index < m_offsets.size());
    ASSERT(m_offsets[index] < m_data.size());
    return &m_data[m_offsets[index]];
}

size_t StringSet::addL(const LChar *s, size_t len) {
    size_t off, idx, hash = hashL(s, len);
    if (!findL(s, len, hash, idx)) {
        off = m_data.size();
        m_data.append(s, len);
        m_data.append('\0');
        idx = m_offsets.size();
        m_offsets.append(off);
        addHash(hash, idx);
    }
    return idx;
}

inline int strncmp(const LChar *s1, const LChar *s2, size_t len) {
    return ::strncmp(reinterpret_cast<const char *>(s1), reinterpret_cast<const char *>(s2), len);
}

bool StringSet::findL(const LChar *s, size_t len, size_t hash, size_t &index) const {
    if (m_hashes.size() == 0)
       return false;
    size_t p = hash % m_hashes.size();
    while (m_hashes[p]) {
        ASSERT(m_hashes[p] < m_offsets.size());
        const LChar *ss = &m_data[m_offsets[m_hashes[p]]];
        if (strncmp(s, ss, len) == 0 && ss[len] == '\0') {
            index = m_hashes[p];
            return true;
        }
        ++p;
        if (p == m_hashes.size())
           p = 0;
    }
    return false;
}

size_t StringSet::hashL(const LChar *s, size_t len) const {
    size_t hash = 5381;
    for (size_t i = 0; i < len; ++i) {
        hash = ((hash << 5) + hash) + static_cast<size_t>(s[i]);
    }
    return hash;
}

size_t StringSet::hashZ(const LChar *s, size_t &length) const {
    size_t hash = 5381, len = 0;
    while (s[len]) {
        hash = ((hash << 5) + hash) + static_cast<size_t>(s[len]);
        ++len;
    }
    length = len;
    return hash;
}

void StringSet::addHash(size_t hash, size_t index) {
    if (2*size() >= m_hashes.size()) {
        size_t i = m_hashes.size();
        m_hashes.resize(2*size() + 1);
        // WTF::Vector not initializing on resize(), wtf ?!
        while (i < m_hashes.size())
            m_hashes[i++] = 0;
        rehashAll();
    }
    addHashNoRehash(hash, index);
}

void StringSet::addHashNoRehash(size_t hash, size_t index) {
    size_t p = hash % m_hashes.size();
    while (m_hashes[p]) {
        ++p;
        if (p == m_hashes.size())
           p = 0;
    }
    m_hashes[p] = index;
}

void StringSet::rehashAll() {
    size_t i, len;
    for (i = 0; i < m_hashes.size(); ++i)
        m_hashes[i] = 0;
    for (i = 1; i < m_offsets.size(); ++i)
        addHashNoRehash(hashZ(&m_data[m_offsets[i]], len), i);
}

} // end namespace
