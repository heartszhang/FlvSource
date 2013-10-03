#pragma once
#include <cstdint>
#include <wrl.h>
#include <mfidl.h>
#include <unknwnbase.h>

typedef Microsoft::WRL::ComPtr < IMFMediaEventQueue>        IMFMediaEventQueuePtr;
typedef Microsoft::WRL::ComPtr < IMFPresentationDescriptor> IMFPresentationDescriptorPtr;
typedef Microsoft::WRL::ComPtr < IMFByteStream>             IMFByteStreamPtr;
typedef Microsoft::WRL::ComPtr < IMFMediaStream>            IMFMediaStreamPtr;
typedef Microsoft::WRL::ComPtr < IMFMediaType>              IMFMediaTypePtr;
typedef Microsoft::WRL::ComPtr < IMFStreamDescriptor>       IMFStreamDescriptorPtr;
typedef Microsoft::WRL::ComPtr < IMFSample>                 IMFSamplePtr;
typedef Microsoft::WRL::ComPtr < IMFMediaBuffer>            IMFMediaBufferPtr;
typedef Microsoft::WRL::ComPtr < IMFAsyncResult>            IMFAsyncResultPtr;
typedef Microsoft::WRL::ComPtr < IMFMediaSource>            IMFMediaSourcePtr;
typedef Microsoft::WRL::ComPtr < IMFMediaTypeHandler>       IMFMediaTypeHandlerPtr;

enum class SourceState : uint32_t
{
  STATE_INVALID,      // Initial state. Have not started opening the stream.
  STATE_OPENING,      // BeginOpen is in progress.
  STATE_STOPPED,
  STATE_PAUSED,
  STATE_STARTED,
  STATE_SHUTDOWN
};

MIDL_INTERFACE("C406054C-AD15-408E-8561-04255272C43B")
IMFMediaSourceExt : public IUnknown
{
  virtual HRESULT STDMETHODCALLTYPE QueueEvent(
  /* [in] */ MediaEventType met,
  /* [in] */ __RPC__in REFGUID guidExtendedType,
  /* [in] */ HRESULT hrStatus,
  /* [unique][in] */ __RPC__in_opt const PROPVARIANT *pvValue) = 0;
  virtual HRESULT STDMETHODCALLTYPE AsyncEndOfStream() = 0;
  virtual HRESULT STDMETHODCALLTYPE AsyncRequestData() = 0;
  virtual HRESULT STDMETHODCALLTYPE Lock() = 0;
  virtual HRESULT STDMETHODCALLTYPE Unlock() = 0;

  virtual HRESULT STDMETHODCALLTYPE BeginOpen(IMFByteStream *pStream, IMFAsyncCallback *, IUnknown *) = 0;
  virtual HRESULT STDMETHODCALLTYPE EndOpen(IMFAsyncResult *pResult) = 0;
};
typedef Microsoft::WRL::ComPtr<IMFMediaSourceExt> IMFMediaSourceExtPtr;

MIDL_INTERFACE("EA4EEBA2-211F-4D54-9AFB-3E050C0FB9E5")
IMFMediaStreamExt : public IUnknown
{
  virtual HRESULT STDMETHODCALLTYPE IsActived()const           = 0;
  virtual HRESULT STDMETHODCALLTYPE Active(BOOL sel) = 0;
  virtual HRESULT STDMETHODCALLTYPE Start(const PROPVARIANT*)  = 0;
  virtual HRESULT STDMETHODCALLTYPE Shutdown()                 = 0;
  virtual HRESULT STDMETHODCALLTYPE NeedsData()                = 0;
  virtual HRESULT STDMETHODCALLTYPE Pause()                    = 0;
  virtual HRESULT STDMETHODCALLTYPE DeliverPayload(IMFSample*) = 0;
  virtual HRESULT STDMETHODCALLTYPE EndOfStream()              = 0;
};
typedef Microsoft::WRL::ComPtr<IMFMediaStreamExt> IMFMediaStreamExtPtr;


inline bool ok(HRESULT const&hr){
  return SUCCEEDED(hr);
}
inline bool fail(HRESULT const&hr){
  return FAILED(hr);
}
