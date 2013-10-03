#pragma once

//没有循环引用的问题
template<class T>// T: Type of the parent object
class AsyncCallback : public IMFAsyncCallback
{
public:
    typedef HRESULT (STDMETHODCALLTYPE T::*InvokeFn)(IMFAsyncResult *pAsyncResult);

    AsyncCallback(T *pParent, InvokeFn fn) : parent(pParent), invoke(fn)
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
    STDMETHODIMP_(ULONG) AddRef() { return parent->AddRef(); }
    STDMETHODIMP_(ULONG) Release() { return parent->Release(); }

    // IMFAsyncCallback methods
    STDMETHODIMP GetParameters(DWORD*, DWORD*)    {        return E_NOTIMPL;    }

    STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult) { return (parent->*invoke)(pAsyncResult); }

    T *parent = nullptr;
    InvokeFn invoke = nullptr;
};
