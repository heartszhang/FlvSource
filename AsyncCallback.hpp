#pragma once

//////////////////////////////////////////////////////////////////////////
//  AsyncCallback [template]
//
//  Description:
//  Helper class that routes IMFAsyncCallback::Invoke calls to a class
//  method on the parent class.
//
//  Usage:
//  Add this class as a member variable. In the parent class constructor,
//  initialize the AsyncCallback class like this:
//      m_cb(this, &CYourClass::OnInvoke)
//  where
//      m_cb       = AsyncCallback object
//      CYourClass = parent class
//      OnInvoke   = Method in the parent class to receive Invoke calls.
//
//  The parent's OnInvoke method (you can name it anything you like) must
//  have a signature that matches the InvokeFn typedef below.
//////////////////////////////////////////////////////////////////////////
#include <Shlwapi.h>
#include <mfapi.h>
#include <comip.h>

#include <functional>
// T: Type of the parent object
template<class T>
class AsyncCallback : public IMFAsyncCallback
{
public:
    typedef HRESULT (T::*InvokeFn)(IMFAsyncResult *pAsyncResult);

    AsyncCallback(T *pParent, InvokeFn fn) : m_pParent(pParent), m_pInvokeFn(fn)
    {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
    {
        static const QITAB qit[] =
        {
            QITABENT(AsyncCallback, IMFAsyncCallback),
            { 0 }
        };
        return QISearch(this, qit, riid, ppv);
    }
    STDMETHODIMP_(ULONG) AddRef() {
        // Delegate to parent class.
        return m_pParent->AddRef();
    }
    STDMETHODIMP_(ULONG) Release() {
        // Delegate to parent class.
        return m_pParent->Release();
    }

    // IMFAsyncCallback methods
    STDMETHODIMP GetParameters(DWORD*, DWORD*)
    {
        // Implementation of this method is optional.
        return E_NOTIMPL;
    }

    STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult)
    {
        return (m_pParent->*m_pInvokeFn)(pAsyncResult);
    }

    T *m_pParent;
    InvokeFn m_pInvokeFn;
};

typedef _com_ptr_t<_com_IIID<IMFAsyncCallback, &__uuidof(IMFAsyncCallback)> > IMFAsyncCallbackPtr;
typedef std::function<HRESULT (IMFAsyncResult*)> invoke_t;
class MFAsyncCallback : public IMFAsyncCallback {
  long refs;
  invoke_t invoke;
  explicit MFAsyncCallback(invoke_t const& fn) :invoke(fn), refs(0)
  {
  }

public:
  static IMFAsyncCallbackPtr New(invoke_t const&fn){
    return IMFAsyncCallbackPtr(new MFAsyncCallback(fn));
  }
  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
  {
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
    if ( r <= 0)
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

inline bool ok(HRESULT const&hr){
  return SUCCEEDED(hr);
}
inline bool fail(HRESULT const&hr){
  return FAILED(hr);
}
