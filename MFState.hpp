#pragma once
#include "MFAsyncCallback.hpp"
template<typename state_t>
class MFState : public IUnknown {
  long refs;
public:
  state_t state;
  explicit MFState(state_t const&v) : state(v), refs(0){}
  MFState() : refs(0){};
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
template<typename state_t>
state_t&
FromAsyncResultState(IMFAsyncResult*result){
  _com_ptr_t<_com_IIID<IUnknown, &__uuidof(IUnknown)>> obj;
  result->GetState(&obj);
  return FromMFState<state_t>(obj);
}

typedef _com_ptr_t<_com_IIID<IUnknown, &__uuidof(IUnknown)>> IMFStatePtr;
template<typename state_t>
IMFStatePtr
NewMFState(state_t const&val){
  auto v = new MFState<state_t>(val);
  return IMFStatePtr(v);
}
