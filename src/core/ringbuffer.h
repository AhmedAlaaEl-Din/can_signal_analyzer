#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace can {

// Fixed-capacity FIFO that overwrites the oldest element when full.
// Every pushed element gets a monotonically increasing sequence number, so
// callers can keep stable references to elements across evictions.
template <typename T>
class RingBuffer
{
public:
    explicit RingBuffer(std::size_t capacity = 100000)
        : m_buffer(capacity == 0 ? 1 : capacity)
    {}

    std::size_t capacity() const noexcept { return m_buffer.size(); }
    std::size_t size() const noexcept { return m_size; }
    bool empty() const noexcept { return m_size == 0; }
    bool full() const noexcept { return m_size == m_buffer.size(); }

    // Sequence number of the oldest stored element / of the next pushed element.
    std::uint64_t firstSeq() const noexcept { return m_nextSeq - m_size; }
    std::uint64_t nextSeq() const noexcept { return m_nextSeq; }

    // Returns true when the push evicted the oldest element.
    bool push(const T &value)
    {
        const std::size_t cap = m_buffer.size();
        m_buffer[(m_start + m_size) % cap] = value;
        ++m_nextSeq;
        if (m_size == cap) {
            m_start = (m_start + 1) % cap;
            return true;
        }
        ++m_size;
        return false;
    }

    // index 0 = oldest element
    const T &at(std::size_t index) const
    {
        assert(index < m_size);
        return m_buffer[(m_start + index) % m_buffer.size()];
    }

    bool containsSeq(std::uint64_t seq) const noexcept
    {
        return seq >= firstSeq() && seq < m_nextSeq;
    }

    const T &bySeq(std::uint64_t seq) const
    {
        assert(containsSeq(seq));
        return at(static_cast<std::size_t>(seq - firstSeq()));
    }

    // Drops all elements. Sequence numbers keep increasing so stale
    // references can never alias new elements.
    void clear() noexcept
    {
        m_start = 0;
        m_size = 0;
    }

    void setCapacity(std::size_t capacity)
    {
        m_buffer.assign(capacity == 0 ? 1 : capacity, T{});
        clear();
    }

private:
    std::vector<T> m_buffer;
    std::size_t m_start = 0;
    std::size_t m_size = 0;
    std::uint64_t m_nextSeq = 0;
};

} // namespace can
