#include <cassert>
#include <windows.h>
#include "buffer.hpp"

uint8_t* buffer::current()
{
    return _ + pointer;
}

void buffer::reset(uint32_t sz) {
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
    return pointer;
}

// Reserves memory for the array, but does not increase the count.
void buffer::allocate(uint32_t alloc)
{
    if (alloc > allocated)
    {
        auto pTmp = new uint8_t[alloc];
//        ZeroMemory(pTmp, alloc);

        assert(pointer <= allocated);

        // Copy the elements to the re-allocated array.
        for (uint32_t i = 0; i < pointer; i++) {
          pTmp[i] = _[i];
        }

        delete[] _;

        _ = pTmp;
        allocated = alloc;

    }
}

void buffer::move_end(uint32_t cb)
{
  pointer += cb;
}
