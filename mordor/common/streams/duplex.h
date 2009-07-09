#ifndef __SINGLEPLEX_STREAM_H__
#define __SINGLEPLEX_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "stream.h"

class DuplexStream : public Stream
{
public:
    DuplexStream(Stream::ptr readParent, Stream::ptr writeParent,
        bool own = true)
        : m_readParent(readParent),
          m_writeParent(writeParent),
          m_own(own)
    {
        assert(readParent);
        assert(writeParent);
        assert(readParent->supportsRead());
        assert(writeParent->supportsWrite());
    }

    Stream::ptr readParent() { return m_readParent; }
    Stream::ptr writeParent() { return m_writeParent; }
    bool ownsParents() { return m_own; }

    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }
    bool supportsSeek() { return m_readParent->supportsSeek() && m_writeParent->supportsSeek(); }
    bool supportsSize() { return m_readParent->supportsSize() && m_writeParent->supportsSize(); }
    bool supportsTruncate() { return m_writeParent->supportsTruncate(); }
    bool supportsFind() { return m_readParent->supportsFind(); }
    bool supportsUnread() { return m_readParent->supportsUnread(); }

    void close(CloseType type = BOTH)
    {
        if (m_own) {
            if (type & READ)
                m_readParent->close(READ);
            if (type & WRITE)
                m_writeParent->close(WRITE);
        }
    }
    size_t read(Buffer &b, size_t len) { return m_readParent->read(b, len); }
    size_t write(const Buffer &b, size_t len) { return m_writeParent->write(b, len); }
    long long seek(long long offset, Anchor anchor)
    {
        assert(supportsSeek());
        long long result1 = m_readParent->seek(offset, anchor);
        long long result2 = m_writeParent->seek(offset, anchor);
        assert(result1 == result2);
        return result1;
    }
    long long size()
    {
        assert(supportsSize());
        long long result1 = m_readParent->size();
        long long result2 = m_writeParent->size();
        assert(result1 == result2);
        return result1;
    }
    void truncate(long long size) { m_writeParent->truncate(size); }
    void flush() { m_writeParent->flush(); }
    size_t find(char delim) { return m_readParent->find(delim); }
    size_t find(const std::string &str, size_t sanitySize = ~0, bool throwOnNotFound = true)
    { return m_readParent->find(str, sanitySize, throwOnNotFound); }
    void unread(const Buffer &b, size_t len) { return m_readParent->unread(b, len); }

protected:
    void readParent(Stream::ptr parent) { m_readParent = parent; }
    void writeParent(Stream::ptr parent) { m_writeParent = parent; }
    void ownsParents(bool own) { m_own = own; }

private:
    Stream::ptr m_readParent, m_writeParent;
    bool m_own;
};

#endif
