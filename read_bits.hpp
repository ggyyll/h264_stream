#ifndef __READ_BITS_HPP__
#define __READ_BITS_HPP__

#include <stdint.h>
#include <assert.h>
#include "noncopyable.hpp"

class ReadBit
{
public:
    ReadBit(const uint8_t *start, uint64_t len)
        : start_(start)
        , length_(len)
        , current_bit_(0) {};
    ~ReadBit() = default;
    unsigned int read_bit()
    {
        assert(current_bit_ <= length_ * 8);
        int index = current_bit_ / 8;
        int offset = current_bit_ % 8 + 1;

        current_bit_++;
        return (start_[index] >> (8 - offset)) & 0x01;
    }
    unsigned int read_n_bits(int n)
    {
        int r = 0;
        int i;
        for (i = 0; i < n; i++)
        {
            r |= (read_bit() << (n - i - 1));
        }
        return r;
    }

    NONCOPYABLE(ReadBit);

private:
    const uint8_t *start_;
    uint64_t length_;
    uint64_t current_bit_;
};



#endif // __READ_BITS_HPP__
