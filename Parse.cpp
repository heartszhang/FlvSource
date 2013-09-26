// #include "FlvSource.h"
#include "Parse.h"
#include <cassert>
#include <mfapi.h>
#include "asynccb.h"

typedef _com_ptr_t < _com_IIID<IMFAsyncResult, &__uuidof(IMFAsyncResult)>> IMFAsyncResultPtr;
HRESULT flv_parser::begin_check_flv_header(IMFByteStreamPtr stream, IMFAsyncCallback*cb, IUnknown*s) {
  this->stream = stream;
  Reserve(flv::flv_file_header_length);
  IMFAsyncResultPtr caller_result;
  MFCreateAsyncResult(nullptr, cb, s, &caller_result);
  auto hr = stream->BeginRead(
    DataPtr(), flv::flv_file_header_length,
      new MFAsyncCallback([this,caller_result](IMFAsyncResult*result)->HRESULT{
          ULONG cb = 0;
          this->stream->EndRead(result,&cb);
          this->MoveEnd(cb);
          auto hr = result->GetStatus();
          if (ok(hr))
            hr = this->check_flv_header();
          caller_result->SetStatus(hr);
          MFInvokeCallback(caller_result.GetInterfacePtr());
          return S_OK;
        }),
      nullptr);
  return hr;
}
HRESULT flv_parser::end_check_flv_header(IMFAsyncResult*result, int32_t*rtl){
  *rtl = result->GetStatus();
  return S_OK;
}

HRESULT flv_parser::begin_tag_header(int8_t withprevfield, IMFAsyncCallback*cb, IUnknown*s){
  DWORD dlen = flv::flv_tag_header_length + withprevfield ? flv::previous_tag_size_field_size : 0;
  Reset(dlen);  //15
  IMFAsyncResultPtr caller_result;
  auto state = new MFState<::tag_header>(); // refs == 0
  auto hr = MFCreateAsyncResult(state, cb, s, &caller_result);
  // if fail, state would be leaked here
  hr = stream->BeginRead(
      DataPtr(),
      dlen,
      new MFAsyncCallback([this, caller_result, withprevfield](IMFAsyncResult*result)->HRESULT{
        DWORD cb = 0;
          auto hr = this->stream->EndRead(result, &cb);
          this->MoveEnd(cb);
          if (ok(hr))
            hr = result->GetStatus();
          if (withprevfield)
            this->previsou_tag_size();
          auto &tagh = FromAsyncResult<::tag_header>(caller_result.GetInterfacePtr());
          if (ok(hr))
            hr = this->tag_header(&tagh);
          caller_result->SetStatus(hr);
          MFInvokeCallback(caller_result.GetInterfacePtr());
          return S_OK;
        }),
      nullptr
                              );
  return S_OK;
}

typedef _com_ptr_t < _com_IIID < IUnknown, &__uuidof(IUnknown)>> IUnknownPtr;
HRESULT flv_parser::end_tag_header(IMFAsyncResult*result, ::tag_header*v){
  auto const&th = FromAsyncResult<::tag_header>(result);
  *v = th;
  return result->GetStatus();
}
HRESULT flv_parser::begin_on_meta_data(DWORD meta_size, IMFAsyncCallback*cb, IUnknown*s){
  Reset(meta_size);
  IMFAsyncResultPtr caller_result;
  auto hr = MFCreateAsyncResult(new MFState<flv_meta>(), cb, s, &caller_result);
  hr = stream->BeginRead(
    DataPtr(), meta_size,
      new MFAsyncCallback([this, caller_result](IMFAsyncResult*result)->HRESULT{
          DWORD cb = 0;
          auto hr = this->stream->EndRead(result, &cb);
          this->MoveEnd(cb);
          if (ok(hr))
            hr = result->GetStatus();
          auto &m = FromAsyncResult<flv_meta>(caller_result.GetInterfacePtr());
          if (ok(hr))
            hr = this->on_meta_data(&m);
          caller_result->SetStatus(hr);
          MFInvokeCallback(caller_result.GetInterfacePtr());
          return S_OK;
        }), nullptr);
  return hr;
}
HRESULT flv_parser::end_on_meta_data(IMFAsyncResult*result, flv_meta*v){
  auto m = FromAsyncResult<flv_meta>(result);
  *v = m;
  return result->GetStatus();
}

buffer::buffer() : m_begin(0), m_end(0), m_count(0), m_allocated(0), m_pArray(NULL)
{
}

//-------------------------------------------------------------------
// Initalize
// Sets the initial buffer size.
//-------------------------------------------------------------------

HRESULT buffer::Initalize(DWORD cbSize)
{
    HRESULT hr = SetSize(cbSize);
    if (SUCCEEDED(hr))
    {
        ZeroMemory(Ptr(), cbSize);
    }
    return hr;
}


//-------------------------------------------------------------------
// DataPtr
// Returns a pointer to the start of the buffer.
//-------------------------------------------------------------------

BYTE* buffer::DataPtr()
{
    return Ptr() + m_begin;
}


//-------------------------------------------------------------------
// DataSize
// Returns the size of the buffer.
//
// Note: The "size" is determined by the start and end pointers.
// The memory allocated for the buffer can be larger.
//-------------------------------------------------------------------

DWORD buffer::DataSize() const
{
    assert(m_end >= m_begin);

    return m_end - m_begin;
}

// Reserves memory for the array, but does not increase the count.
HRESULT buffer::Allocate(DWORD alloc)
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
HRESULT buffer::SetSize(DWORD count)
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

HRESULT buffer::Reserve(DWORD cb)
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

HRESULT buffer::MoveStart(DWORD cb)
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

HRESULT buffer::MoveEnd(DWORD cb)
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

DWORD buffer::CurrentFreeSize() const
{
    assert(m_count >= DataSize());
    return m_count - DataSize();
}
