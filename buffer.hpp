#pragma once
// Buffer class:
// Resizable buffer used to hold the FLV data.
struct buffer
{
  buffer();
  ~buffer()
    {
        delete [] m_pArray;
    }
    HRESULT Initalize(DWORD cbSize);

    BYTE*   DataPtr();
    DWORD   DataSize() const;

    // Reserve: Reserves cb bytes of free data in the buffer.
    // The reserved bytes start at DataPtr() + DataSize().
    HRESULT Reserve(DWORD cb);

    // MoveStart: Moves the front of the buffer.
    // Call this method after consuming data from the buffer.
    HRESULT MoveStart(DWORD cb);

    // MoveEnd: Moves the end of the buffer.
    // Call this method after reading data into the buffer.
    HRESULT MoveEnd(DWORD cb);

    HRESULT Reset(DWORD cb);  // reset buffer and reserve
private:
    BYTE* Ptr() { return m_pArray; }

    HRESULT SetSize(DWORD count);
    HRESULT Allocate(DWORD alloc);

    DWORD   CurrentFreeSize() const;

private:

    BYTE       *m_pArray;
    DWORD   m_count;        // Nominal count.
    DWORD   m_allocated;    // Actual allocation size.

    DWORD   m_begin;
    DWORD   m_end;  // 1 past the last element
};
