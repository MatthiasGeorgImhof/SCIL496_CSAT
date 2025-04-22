#ifndef CHECKSUM_HPP
#define CHECKSUM_HPP

#include <cstdint>
#include <cstddef>

typedef uint32_t crc_t;

class ChecksumCalculator {
public:
  ChecksumCalculator(crc_t initial_checksum = 0) : checksum_(initial_checksum) {}

  void update(const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
      checksum_ ^= data[i]; // XOR checksum (replace with your desired algorithm)
    }
  }

  crc_t get_checksum() const { return checksum_; }

  void reset(crc_t initial_checksum = 0) { checksum_ = initial_checksum; }

private:
crc_t checksum_;
};

#endif // CHECKSUM_HPP