//////////////////////////////////////////////////////////////////////////
//
// FlvByteStreamHandler.cpp
// Implements the byte-stream handler for the Flv source.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////


#include "FlvSource.h"
#include "FlvByteStreamHandler.h"


//-------------------------------------------------------------------
// FlvByteStreamHandler  class
//-------------------------------------------------------------------


//-------------------------------------------------------------------
// CreateInstance
// Static method to create an instance of the oject.
//
// This method is used by the class factory.
//
//-------------------------------------------------------------------

HRESULT FlvByteStreamHandler::CreateInstance(REFIID iid, void **ppv)
{
    if (ppv == NULL)
    {
        return E_POINTER;
    }

    HRESULT hr = S_OK;

    FlvByteStreamHandler *pHandler = new (std::nothrow) FlvByteStreamHandler(hr);
    if (pHandler == NULL)
    {
        return E_OUTOFMEMORY;
    }

    if (SUCCEEDED(hr))
    {
        hr = pHandler->QueryInterface(iid, ppv);
    }

    SafeRelease(&pHandler);
    return hr;
}


//-------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------

FlvByteStreamHandler::FlvByteStreamHandler(HRESULT& hr)
    : m_cRef(1), m_pSource(NULL), m_pResult(NULL)
{
    DllAddRef();
}

//-------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------

FlvByteStreamHandler::~FlvByteStreamHandler()
{
    SafeRelease(&m_pSource);
    SafeRelease(&m_pResult);

    DllRelease();
}


//-------------------------------------------------------------------
// IUnknown methods
//-------------------------------------------------------------------

ULONG FlvByteStreamHandler::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG FlvByteStreamHandler::Release()
{
    LONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        delete this;
    }
    return cRef;
}

HRESULT FlvByteStreamHandler::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(FlvByteStreamHandler, IMFByteStreamHandler),
        QITABENT(FlvByteStreamHandler, IMFAsyncCallback),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}


//-------------------------------------------------------------------
// IMFByteStreamHandler methods
//-------------------------------------------------------------------

//-------------------------------------------------------------------
// BeginCreateObject
// Starts creating the media source.
//-------------------------------------------------------------------

HRESULT FlvByteStreamHandler::BeginCreateObject(
        /* [in] */ IMFByteStream *pByteStream,
        /* [in] */ LPCWSTR pwszURL,
        /* [in] */ DWORD dwFlags,
        /* [in] */ IPropertyStore *pProps,
        /* [out] */ IUnknown **ppIUnknownCancelCookie,  // Can be NULL
        /* [in] */ IMFAsyncCallback *pCallback,
        /* [in] */ IUnknown *punkState                  // Can be NULL
        )
{
    if (pByteStream == NULL)
    {
        return E_POINTER;
    }

    if (pCallback == NULL)
    {
        return E_POINTER;
    }

    if ((dwFlags & MF_RESOLUTION_MEDIASOURCE) == 0)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = S_OK;

    IMFAsyncResult *pResult = NULL;
    FlvSource    *pSource = NULL;

    // Create an instance of the media source.
    hr = FlvSource::CreateInstance(&pSource);

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
        hr = pSource->BeginOpen(pByteStream, this, NULL);
    }

    if (SUCCEEDED(hr))
    {
        if (ppIUnknownCancelCookie)
        {
            *ppIUnknownCancelCookie = NULL;
        }

        m_pSource = pSource;
        m_pSource->AddRef();

        m_pResult = pResult;
        m_pResult->AddRef();
    }

// cleanup
    SafeRelease(&pSource);
    SafeRelease(&pResult);
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

        assert(m_pSource != NULL);

        hr = m_pSource->QueryInterface(IID_PPV_ARGS(ppObject));
    }

    SafeRelease(&m_pSource);
    SafeRelease(&m_pResult);
    return hr;
}


HRESULT FlvByteStreamHandler::CancelObjectCreation(IUnknown *pIUnknownCancelCookie)
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
    HRESULT hr = S_OK;

    if (m_pSource)
    {
        hr = m_pSource->EndOpen(pResult);
    }
    else
    {
        hr = E_UNEXPECTED;
    }

    m_pResult->SetStatus(hr);

    hr = MFInvokeCallback(m_pResult);

    return hr;
}
