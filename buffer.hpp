#pragma once
#include <cstdint>

struct buffer
{
  buffer()              = default;
  buffer(buffer const&) = delete;
  buffer&operator= (buffer const&) = delete;
  ~buffer()
  {
    delete[] _;
  }
  uint8_t*   current();
  uint32_t   size() const;

  // move_end: Moves the end of the buffer.
  // Call this method after reading data into the buffer.
  void move_end(uint32_t cb);
  void reset(uint32_t cb);  // reset buffer and reserve
private:
  void      allocate(uint32_t alloc);
  uint8_t  *_         = nullptr;
  uint32_t  allocated = 0;              // Actual allocation size.
  uint32_t  pointer   = 0;              // 1 past the last element
};
