#include <cassert>
#include <windows.h>
#include "buffer.hpp"

uint8_t* buffer::current()
{
    return _ + pointer;
}

void buffer::reset(uint32_t sz) {
  count = 0;
  pointer = 0;
  allocate(sz);
}

//-------------------------------------------------------------------
// size
// Returns the size of the buffer.
//
// Note: The "size" is determined by the start and end pointers.
// The memory allocated for the buffer can be larger.
//-------------------------------------------------------------------

uint32_t buffer::size() const
{
    return count - pointer;
}

// Reserves memory for the array, but does not increase the count.
void buffer::allocate(uint32_t alloc) {
  if (alloc > allocated) {
    auto tmp = new uint8_t[alloc];
    assert(pointer <= allocated);
    memcpy_s(tmp, alloc, _, allocated);
    delete[] _;
    _ = tmp;
    allocated = alloc;
  }
}

void buffer::move_end(uint32_t cb)
{
  count += cb;
}
