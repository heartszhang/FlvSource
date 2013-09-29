#pragma once


//-------------------------------------------------------------------
// FlvByteStreamHandler  class
//
// Byte-stream handler for Flv streams.
//-------------------------------------------------------------------

class FlvByteStreamHandler
    : public IMFByteStreamHandler,
      public IMFAsyncCallback
{
public:
    static HRESULT CreateInstance(REFIID iid, void **ppMEG);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IMFAsyncCallback
    STDMETHODIMP GetParameters(DWORD* /*pdwFlags*/, DWORD* /*pdwQueue*/)
    {
        // Implementation of this method is optional.
        return E_NOTIMPL;
    }

    STDMETHODIMP Invoke(IMFAsyncResult* pResult);

    // IMFByteStreamHandler
    STDMETHODIMP BeginCreateObject(
            /* [in] */ IMFByteStream *pByteStream,
            /* [in] */ LPCWSTR pwszURL,
            /* [in] */ DWORD dwFlags,
            /* [in] */ IPropertyStore *pProps,
            /* [out] */ IUnknown **ppIUnknownCancelCookie,
            /* [in] */ IMFAsyncCallback *pCallback,
            /* [in] */ IUnknown *punkState);

    STDMETHODIMP EndCreateObject(
            /* [in] */ IMFAsyncResult *pResult,
            /* [out] */ MF_OBJECT_TYPE *pObjectType,
            /* [out] */ IUnknown **ppObject);

    STDMETHODIMP CancelObjectCreation(IUnknown *pIUnknownCancelCookie);
    STDMETHODIMP GetMaxNumberOfBytesRequiredForResolution(QWORD* pqwBytes);

private:

    FlvByteStreamHandler(HRESULT& hr);
    ~FlvByteStreamHandler();

    long            m_cRef; // reference count
    FlvSource     *m_pSource;
    IMFAsyncResult  *m_pResult;
};

inline HRESULT FlvByteStreamHandler_CreateInstance(REFIID riid, void **ppv)
{
    return FlvByteStreamHandler::CreateInstance(riid, ppv);
}