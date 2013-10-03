#include "FlvSource.h"
#include "FlvByteStreamHandler.h"

//-------------------------------------------------------------------
// BeginCreateObject
// Starts creating the media source.
//-------------------------------------------------------------------

HRESULT FlvByteStreamHandler::BeginCreateObject(
        /* [in] */ IMFByteStream *pByteStream,
        /* [in] */ LPCWSTR /*pwszURL*/,
        /* [in] */ DWORD dwFlags,
        /* [in] */ IPropertyStore * /*pProps*/,
        /* [out] */ IUnknown **ppIUnknownCancelCookie,  // Can be NULL
        /* [in] */ IMFAsyncCallback *pCallback,
        /* [in] */ IUnknown *punkState                  // Can be NULL
        )
{
    if (pByteStream == NULL || pCallback == NULL)
    {
        return E_POINTER;
    }

    if ((dwFlags & MF_RESOLUTION_MEDIASOURCE) == 0)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = S_OK;

    IMFAsyncResultPtr pResult = NULL;
    IMFMediaSourceExtPtr src;

    // Create an instance of the media source.
    hr = MakeAndInitialize<FlvSource>(&src);

    // Create a result object for the caller's async callback.
    if (SUCCEEDED(hr))
    {
        hr = MFCreateAsyncResult(NULL, pCallback, punkState, &pResult);
    }

    // Start opening the source. This is an async operation.
    // When it completes, the source will invoke our callback
    // and then we will invoke the caller's callback.
    if (SUCCEEDED(hr))
    {
      hr = src->BeginOpen(pByteStream, this, NULL);
    }

    if (SUCCEEDED(hr))
    {
        if (ppIUnknownCancelCookie)
        {
            *ppIUnknownCancelCookie = NULL;
        }

        source = src;

        m_pResult = pResult;
    }

    return hr;
}

//-------------------------------------------------------------------
// EndCreateObject
// Completes the BeginCreateObject operation.
//-------------------------------------------------------------------

HRESULT FlvByteStreamHandler::EndCreateObject(
        /* [in] */ IMFAsyncResult *pResult,
        /* [out] */ MF_OBJECT_TYPE *pObjectType,
        /* [out] */ IUnknown **ppObject)
{
    if (pResult == NULL || pObjectType == NULL || ppObject == NULL)
    {
        return E_POINTER;
    }

    HRESULT hr = S_OK;

    *pObjectType = MF_OBJECT_INVALID;
    *ppObject = NULL;

    hr = pResult->GetStatus();

    if (SUCCEEDED(hr))
    {
        *pObjectType = MF_OBJECT_MEDIASOURCE;
// todo: shoud use query-interface to iunknown?        ComPtr<IUnknown> p;
//        hr = source.As(&p);
        *ppObject = source.Detach();
    }

    source = nullptr;
    source = nullptr;
    return hr;
}


HRESULT FlvByteStreamHandler::CancelObjectCreation(IUnknown * /*pIUnknownCancelCookie*/)
{
    return E_NOTIMPL;
}

HRESULT FlvByteStreamHandler::GetMaxNumberOfBytesRequiredForResolution(QWORD* pqwBytes)
{
  *pqwBytes = 9;
    return S_OK;
}


//-------------------------------------------------------------------
// Invoke
// Callback for the media source's BeginOpen method.
//-------------------------------------------------------------------

HRESULT FlvByteStreamHandler::Invoke(IMFAsyncResult* pResult)
{
    assert(source);
    auto hr = source->EndOpen(pResult);

    m_pResult->SetStatus(hr);

    hr = MFInvokeCallback(m_pResult.Get());

    return hr;
}
