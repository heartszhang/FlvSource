#pragma once
#include <cstdint>
// Buffer class:
// Resizable buffer used to hold the FLV data.
struct buffer
{
  buffer() = default;
  ~buffer()
  {
    delete[] m_pArray;
  }
  uint8_t*   DataPtr();
  uint32_t   DataSize() const;

  // Reserve: Reserves cb bytes of free data in the buffer.
  // The reserved bytes start at DataPtr() + DataSize().
  HRESULT Reserve(uint32_t cb);

  // MoveStart: Moves the front of the buffer.
  // Call this method after consuming data from the buffer.
  HRESULT MoveStart(uint32_t cb);

  // MoveEnd: Moves the end of the buffer.
  // Call this method after reading data into the buffer.
  HRESULT MoveEnd(uint32_t cb);

  void Reset(uint32_t cb);  // reset buffer and reserve
private:
  uint8_t*    Ptr() { return m_pArray; }

  HRESULT     SetSize(uint32_t count);
  HRESULT     Allocate(uint32_t alloc);

  uint32_t    CurrentFreeSize() const;

private:

  uint8_t    *m_pArray = nullptr;
  uint32_t   m_count = 0;        // Nominal count.
  uint32_t   m_allocated = 0;    // Actual allocation size.

  uint32_t   m_begin = 0;
  uint32_t   m_end = 0;  // 1 past the last element
};
