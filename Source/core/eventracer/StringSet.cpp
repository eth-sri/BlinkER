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
    m_size = 1;
    m_data.append('\0');
    m_hashes.resize(33);
    for (size_t i = 0; i < m_hashes.size(); ++i)
        m_hashes[i] = 0;
}

// Adds a string to the set. Returns the index of the added string. Duplicate
// strings get identical indices.
size_t StringSet::put(const WTF::String &s, bool &added) {
    CString tmp = s.utf8();
    return addL(tmp.data(), tmp.length(), added);
}

// Returns a copy of the string for an index.
WTF::String StringSet::get(size_t index) const {
    return WTF::String::fromUTF8(peek(index));
}

// Returns whether the set contains a given string.
bool StringSet::contains(const WTF::String &s) const {
    size_t index;
    return contains(s, index);
}

// Returns whether the set contains a given string.
bool StringSet::contains(const WTF::String &s, size_t &index) const {
    CString tmp = s.utf8();
    size_t hash = hashL(tmp.data(), tmp.length());
    return findL(tmp.data(), tmp.length(), hash, index);
}

// Returns the string for an index. The returned pointer is guaranteed
// to be valid only until the next modification of StringSet.
const char *StringSet::peek(size_t index) const {
    ASSERT(index < m_data.size());
    return &m_data[index];
}

size_t StringSet::addL(const char *s, size_t len, bool &added) {
    size_t idx, hash = hashL(s, len);
    added = false;
    if (!findL(s, len, hash, idx)) {
        idx = m_data.size();
        m_data.append(s, len);
        m_data.append('\0');
        addHash(hash, idx);
        added = true;
        ++m_size;
    }
    return idx;
}

bool StringSet::findL(const char *s, size_t len, size_t hash, size_t &index) const {
    if (m_hashes.size() == 0)
       return false;
    size_t p = hash % m_hashes.size();
    while (m_hashes[p]) {
        ASSERT(m_hashes[p] < m_data.size());
        const char *ss = &m_data[m_hashes[p]];
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

size_t StringSet::hashL(const char *s, size_t len) const {
    size_t hash = 5381;
    for (size_t i = 0; i < len; ++i) {
        hash = ((hash << 5) + hash) + static_cast<size_t>(s[i]);
    }
    return hash;
}

size_t StringSet::hashZ(const char *s, size_t &length) const {
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
    for (size_t index = 1; index < m_data.size(); index += len + 1) {
        const char *p = &m_data[index];
        addHashNoRehash(hashZ(p, len), index);
    }
}

} // end namespace
