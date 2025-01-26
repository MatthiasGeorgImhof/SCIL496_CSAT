#ifndef INC_CIRCULARBUFFER_HPP_
#define INC_CIRCULARBUFFER_HPP_

#include <array>
#include <cstddef>
#include <utility> 

template <typename T, size_t capacity_>
class CircularBuffer
{
public:
    CircularBuffer() : head_(0), tail_(0), count_(0) {}

    void push(T value)
    { 
        data[head_] = value;
        head_ = (head_ + 1) % capacity_;
        ++count_;
    }

    T pop()
    { 
        size_t current = tail_;
        tail_ = (tail_ + 1) % capacity_;
        --count_;
        return std::move(data[current]);
    }

    const T peek()
    { 
        size_t current = tail_;
        return data[current];
    }

    bool empty() const
    {
        return count_ == 0;
    }

    bool full() const
    {
        return count_ == capacity_;
    }

    size_t size() const
    {
        return count_;
    }

    size_t capacity() const
    {
        return capacity_;
    }

private:
    std::array<T, capacity_> data;
    size_t head_;
    size_t tail_;
    size_t count_;
};

#endif /* INC_CIRCULARBUFFER_HPP_ */
