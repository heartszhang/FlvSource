#pragma once
#include <Shlwapi.h>    //QITABENT
#include <mfapi.h>      //IMFAsyncCallback
#include <comip.h>      //_com_ptr_t
#include <functional>   // std::function

typedef _com_ptr_t<_com_IIID<IMFAsyncCallback, &__uuidof(IMFAsyncCallback)> > IMFAsyncCallbackPtr;
typedef std::function<HRESULT(IMFAsyncResult*)> invoke_t;
class MFAsyncCallback : public IMFAsyncCallback {
  long     refs;
  invoke_t invoke;
  explicit MFAsyncCallback(invoke_t const& fn) :invoke(fn), refs(0)  {
  }

public:
  static IMFAsyncCallbackPtr New(invoke_t const&fn){
    return IMFAsyncCallbackPtr(new MFAsyncCallback(fn));
  }
  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv)  {
    static const QITAB qit[] =
    {
      QITABENT(MFAsyncCallback, IMFAsyncCallback),
      { 0 }
    };
    return QISearch(this, qit, riid, ppv);
  }

  STDMETHODIMP_(ULONG) AddRef() {
    return InterlockedIncrement(&refs);
  }

  STDMETHODIMP_(ULONG) Release() {
    auto r = InterlockedDecrement(&refs);
    if (r <= 0)
      delete this;
    return r;
  }

  // IMFAsyncCallback methods
  STDMETHODIMP GetParameters(DWORD*, DWORD*)
  {
    // Implementation of this method is optional.
    return E_NOTIMPL;
  }

  STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult)
  {
    return invoke(pAsyncResult);
  }
};
