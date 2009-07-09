// Copyright (c) 2009 - Decho Corp.

#include "memory.h"

#include <stdexcept>

#ifdef min
#undef min
#endif

MemoryStream::MemoryStream()
: m_offset(0)
{}

MemoryStream::MemoryStream(const Buffer &b)
: m_read(b),
  m_original(b),
  m_offset(0)
{}

size_t
MemoryStream::read(Buffer &b, size_t len)
{
    size_t todo = std::min(len, m_read.readAvailable());
    b.copyIn(m_read, todo);
    m_read.consume(todo);
    m_offset += todo;
    return todo;
}

size_t
MemoryStream::write(const Buffer &b, size_t len)
{
    return writeInternal(b, len);
}

size_t
MemoryStream::write(const void *b, size_t len)
{
    return writeInternal(b, len);
}

template <class T>
size_t
MemoryStream::writeInternal(T b, size_t len)
{
    size_t size = m_original.readAvailable();
    if (m_offset == size) {
        m_original.copyIn(b, len);
        m_offset += len;
    } else if (m_offset > size) {
        // extend the stream, then write
        truncate(m_offset);
        m_original.copyIn(b, len);
        m_offset += len;
    } else {
        // write at offset
        Buffer original(m_original);
        // Re-copy in to orig all data before the write
        m_original.clear();
        m_original.copyIn(original, m_offset);
        original.consume(m_offset);
        // copy in the write, advancing the stream pointer
        m_original.copyIn(b, len);
        m_offset += len;
        if (m_offset < size) {
            original.consume(len);
            // Copy in any remaining data beyond the write
            m_original.copyIn(original, original.readAvailable());
            // Reset our read buffer to the current stream pos
            m_read.clear();
            m_read.copyIn(original);
        }
    }
    return len;
}

long long
MemoryStream::seek(long long offset, Anchor anchor)
{
    size_t size = m_original.readAvailable();

    switch (anchor) {
        case BEGIN:
            if (offset < 0)
                throw std::invalid_argument("resulting offset is negative");
            if ((unsigned long long)offset > (size_t)~0) {
                throw std::invalid_argument(
                    "Memory stream position cannot exceed virtual address space.");
            }
            m_read.clear();
            m_read.copyIn(m_original, m_original.readAvailable());
            m_offset = (size_t)offset;
            m_read.consume(std::min((size_t)offset, size));
            return (long long)m_offset;
        case CURRENT:
            if (offset < 0) {
                return seek(m_offset + offset, BEGIN);
            } else {
                // Optimized forward seek
                if (m_offset + offset > (size_t)~0) {
                    throw std::invalid_argument(
                        "Memory stream position cannot exceed virtual address space.");
                }
                if (m_offset <= size) {
                    m_read.consume(std::min((size_t)offset, size - m_offset));
                    m_offset += (size_t)offset;
                }
                return (long long)m_offset;
            }
        case END:
            // Change this into a CURRENT to try and catch an optimized forward
            // seek
            return seek(size + offset - m_offset, CURRENT);
        default:
            assert(false);
            return 0;
    }
}

long long
MemoryStream::size()
{
    return (long long)m_original.readAvailable();
}

void
MemoryStream::truncate(long long size)
{
    assert(size >= 0);
    if ((unsigned long long)size > (size_t)~0) {
        throw std::invalid_argument(
            "Memory stream size cannot exceed virtual address space.");
    }
    size_t currentSize = m_original.readAvailable();

    if (currentSize == (size_t)size) {
    } else if (currentSize > (size_t)size) {
        Buffer original(m_original);
        m_original.clear();
        m_original.copyIn(original, (size_t)size);
        m_read.clear();
        m_read.copyIn(m_original, (size_t)size);
        m_read.consume(std::min(m_offset, (size_t)size));
    } else {
        size_t needed = (size_t)size - currentSize;
        m_original.reserve(needed);
        std::vector<iovec> iovs = m_original.writeBufs(needed);
        for (std::vector<iovec>::iterator it(iovs.begin());
            it != iovs.end();
            ++it) {
            memset(it->iov_base, 0, it->iov_len);
        }
        m_original.produce(needed);
        // Reset the read buf so we're referencing the same memory
        m_read.clear();
        m_read.copyIn(m_original);
        m_read.consume(std::min(m_offset, (size_t)size));
    }

    assert(m_original.readAvailable() == (size_t)size);
}

size_t
MemoryStream::find(char delim)
{
    ptrdiff_t result = m_read.find(delim);
    if (result != -1)
        return result;
    throw std::runtime_error("Unexpected EOF");
}

size_t
MemoryStream::find(const std::string &str, size_t sanitySize, bool throwIfNotFound)
{
    ptrdiff_t result = m_read.find(str);
    if (result != -1)
        return result;
    if (throwIfNotFound)
        throw std::runtime_error("Unexpected EOF");
    return ~0;
}
