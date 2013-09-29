#include <cassert>
#include <windows.h>
#include "buffer.hpp"

buffer::buffer() : m_begin(0),
                   m_end(0),
                   m_count(0),
                   m_allocated(0),
                   m_pArray(NULL)
{
}

//-------------------------------------------------------------------
// DataPtr
// Returns a pointer to the start of the buffer.
//-------------------------------------------------------------------

BYTE* buffer::DataPtr()
{
    return Ptr() + m_begin;
}

void buffer::Reset(uint32_t sz){
  m_count = 0;
  m_begin = m_end = 0;
  Allocate(sz);
}

//-------------------------------------------------------------------
// DataSize
// Returns the size of the buffer.
//
// Note: The "size" is determined by the start and end pointers.
// The memory allocated for the buffer can be larger.
//-------------------------------------------------------------------

uint32_t buffer::DataSize() const
{
    assert(m_end >= m_begin);

    return m_end - m_begin;
}

// Reserves memory for the array, but does not increase the count.
HRESULT buffer::Allocate(uint32_t alloc)
{
    HRESULT hr = S_OK;
    if (alloc > m_allocated)
    {
        BYTE *pTmp = new BYTE[alloc];
        if (pTmp)
        {
            ZeroMemory(pTmp, alloc);

            assert(m_count <= m_allocated);

            // Copy the elements to the re-allocated array.
            for (DWORD i = 0; i < m_count; i++)
            {
                pTmp[i] = m_pArray[i];
            }

            delete [] m_pArray;

            m_pArray = pTmp;
            m_allocated = alloc;
        }
        else
        {
            hr = E_OUTOFMEMORY;
        }
    }
    return hr;
}

// Changes the count, and grows the array if needed.
HRESULT buffer::SetSize(uint32_t count)
{
    assert (m_count <= m_allocated);

    HRESULT hr = S_OK;
    if (count > m_allocated)
    {
        hr = Allocate(count);
    }
    if (SUCCEEDED(hr))
    {
        m_count = count;
    }
    return hr;
}


//-------------------------------------------------------------------
// Reserve
// Reserves additional bytes of memory for the buffer.
//
// This method does *not* increase the value returned by DataSize().
//
// After this method returns, the value of DataPtr() might change,
// so do not cache the old value.
//-------------------------------------------------------------------

HRESULT buffer::Reserve(uint32_t cb)
{
    if (cb > MAXDWORD - DataSize())
    {
        return E_INVALIDARG; // Overflow
    }

    HRESULT hr = S_OK;

    // If this would push the end position past the end of the array,
    // then we need to copy up the data to start of the array. We might
    // also need to realloc the array.

    if (cb > m_count - m_end)
    {
        // New end position would be past the end of the array.
        // Check if we need to grow the array.

        if (cb > CurrentFreeSize())
        {
            // Array needs to grow
            hr = SetSize(DataSize() + cb);
        }

        if (SUCCEEDED(hr))
        {
            MoveMemory(Ptr(), DataPtr(), DataSize());

            // Reset begin and end.
            m_end = DataSize(); // Update m_end first before resetting m_begin!
            m_begin = 0;
        }
    }

    assert(CurrentFreeSize() >= cb);

    return hr;
}


//-------------------------------------------------------------------
// MoveStart
// Moves the start of the buffer by cb bytes.
//
// Call this method after consuming data from the buffer.
//-------------------------------------------------------------------

HRESULT buffer::MoveStart(uint32_t cb)
{
    // Cannot advance pass the end of the buffer.
    if (cb > DataSize())
    {
        return E_INVALIDARG;
    }

    m_begin += cb;
    return S_OK;
}


//-------------------------------------------------------------------
// MoveEnd
// Moves end position of the buffer.
//-------------------------------------------------------------------

HRESULT buffer::MoveEnd(uint32_t cb)
{
    HRESULT hr = S_OK;

    hr = Reserve(cb);

    if (SUCCEEDED(hr))
    {
        m_end += cb;
    }
    return hr;
}


//-------------------------------------------------------------------
// CurrentFreeSize (private)
//
// Returns the size of the array minus the size of the data.
//-------------------------------------------------------------------

uint32_t buffer::CurrentFreeSize() const
{
    assert(m_count >= DataSize());
    return m_count - DataSize();
}
