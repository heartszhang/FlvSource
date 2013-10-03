#pragma once
#include <wrl.h>
using namespace Microsoft::WRL;

// Byte-stream handler for Flv streams.
class __declspec(uuid("EFE6208A-0A2C-49fa-8A01-3768B559B6DA"))
FlvByteStreamHandler
  : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMFByteStreamHandler, IMFAsyncCallback>
{
public:
  FlvByteStreamHandler() = default;
    // IMFAsyncCallback
  STDMETHODIMP GetParameters(DWORD* /*pdwFlags*/, DWORD* /*pdwQueue*/)
  {
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
  ~FlvByteStreamHandler() = default;

  IMFMediaSourceExtPtr source;
  IMFAsyncResultPtr    result;
};

CoCreatableClass(FlvByteStreamHandler);