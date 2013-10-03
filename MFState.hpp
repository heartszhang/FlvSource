#pragma once
#include <wrl\implements.h>
#include "MFAsyncCallback.hpp"

using namespace Microsoft::WRL;

template<typename state_t>
class MFState : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IUnknown> {
public:
  state_t state;
  MFState(){};
  HRESULT RuntimeClassInitialize(state_t const&v)
  {
    state = v;
    return S_OK;
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
  ComPtr<IUnknown> obj;
  result->GetObject(&obj);
  return FromMFState<state_t>(obj.Get());
}

template<typename state_t>
state_t&
FromAsyncResultState(IMFAsyncResult*result){
  ComPtr<IUnknown> obj;
  result->GetState(&obj);
  return FromMFState<state_t>(obj.Get());
}

typedef ComPtr<IUnknown> MFStatePtr;
template<typename state_t>
MFStatePtr
NewMFState(state_t const&val){
  MFStatePtr obj;
  auto hr = MakeAndInitialize<MFState<state_t>>(&obj, val);  // ignore hresult
  hr;
  return obj;
}
