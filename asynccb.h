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

template<typename state_t>
class MFState : public IUnknown {
  long refs;
public:
  state_t state;
  explicit MFState(state_t const&v) : state(v){}
  MFState(){}
  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
  {
    static const QITAB qit[] =
        {
          QITABENT(MFAsyncCallback, IUnknown),
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
};
template<typename state_t>
state_t&
FromMFState(IUnknown*u){
  auto pthis = static_cast<MFState<state_t>*>(u);
  return pthis->state;
}

template<typename state_t>
state_t&
FromAsyncResult(IMFAsyncResult*result){
  _com_ptr_t<_com_IIID<IUnknown,&__uuidof(IUnknown)>> obj;
  result->GetObject(&obj);
  return FromMFState<state_t>(obj);
}

typedef _com_ptr_t<_com_IIID<IUnknown, &__uuidof(IUnknown)>> IMFStatePtr;
template<typename state_t>
IMFStatePtr
NewMFState(state_t const&val){
  auto v = new MFState<state_t>(val);
  return IMFStatePtr(v);
}

typedef std::function<HRESULT (IMFAsyncResult*)> invoke_t;
class MFAsyncCallback : public IMFAsyncCallback {
  long refs;
  invoke_t invoke;
public:
  explicit MFAsyncCallback(invoke_t const& fn) :invoke(fn)
  {
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
