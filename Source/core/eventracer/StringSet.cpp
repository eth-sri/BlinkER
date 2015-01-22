/*
 * StringSet.cpp
 *
 *  Created on: May 10, 2012
 *      Author: veselin
 */

#include "config.h"
#include "StringSet.h"
#include <string.h>
#include <stdarg.h>
#include "EventRacerLogClient.h"

namespace blink {

StringSet::StringSet() {
    m_size = 1;
    m_data.append('\0');
    m_hashes.resize(32);
    m_hashes.fill(0);
}

// Adds a string to the set. Returns the index of the added string. Duplicate
// strings get identical indices.
size_t StringSet::put(const WTF::String &s) {
    CString tmp = s.utf8();
    return put(tmp.data(), tmp.length());
}

// Formats and interns a string.
size_t StringSet::putf(const char *fmt, ...) {
    va_list ap;
    Vector<char, 64> buf(64);

    va_start(ap, fmt);
    int len = vsnprintf(buf.data(), buf.size(), fmt, ap);
    va_end(ap);
    if (len < 0)
        return 0;
    if (static_cast<size_t>(len) >= buf.size()) {
        buf.resize(len + 1);
        va_start(ap, fmt);
        len = vsnprintf(buf.data(), buf.size(), fmt, ap);
        va_end(ap);
    }

    return put(buf.data(), len);
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

size_t StringSet::put(const char *s, size_t len) {
    size_t idx, hash = hashL(s, len);
    if (!findL(s, len, hash, idx)) {
        idx = m_data.size();
        m_data.append(s, len);
        m_data.append('\0');
        addHash(hash, idx);
        ++m_size;
    }
    return idx;
}

bool StringSet::findL(const char *s, size_t len, size_t hash, size_t &index) const {
    size_t cnt = 0;
    size_t p = hash & (m_hashes.size() - 1);
    while (m_hashes[p]) {
        ASSERT(m_hashes[p] < m_data.size());
        const char *ss = &m_data[m_hashes[p]];
        if (strncmp(s, ss, len) == 0 && ss[len] == '\0') {
            index = m_hashes[p];
            return true;
        }
        if (++cnt == m_hashes.size())
            break;
        p = (p + cnt) & (m_hashes.size() - 1);
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
    if (2 * size() >= m_hashes.size()) {
        do {
            m_hashes.resize(m_hashes.size() * 2);
        } while (2 * size() >= m_hashes.size());
        rehashAll();
    }
    addHashNoRehash(hash, index);
}

void StringSet::addHashNoRehash(size_t hash, size_t index) {
    size_t cnt = 0;
    size_t p = hash & (m_hashes.size() - 1);
    while (m_hashes[p]) {
        ++cnt;
        p = (p + cnt) & (m_hashes.size() - 1);
    }
    m_hashes[p] = index;
}

void StringSet::rehashAll() {
    size_t i, len;
    m_hashes.fill(0);
    for (i = 1; i < m_data.size(); i += len + 1) {
        const char *p = &m_data[i];
        addHashNoRehash(hashZ(p, len), i);
    }
}


StringSetWithFlush::StringSetWithFlush() : m_pending(1) {}

void StringSetWithFlush::flush(size_t kind, EventRacerLogClient *clnt) {
    if (m_pending < m_data.size()) {
        size_t pos = m_pending;
        WTF::Vector<WTF::String> v;
        while(pos < m_data.size()) {
            WTF::String s = get(pos);
            v.append(s);
            pos += s.length() + 1;
        }
        clnt->didUpdateStringTable(kind, v);
        m_pending = pos;
        ASSERT(m_pending == m_data.size());
    }
}

} // end namespace
