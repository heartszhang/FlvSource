#pragma once
#include <wrl\implements.h>
#include <mfidl.h>      //IMFMediaStream
#include <mferror.h>
#include "MFMediaSourceExt.hpp"
#include "InterfaceList.hpp"
using namespace Microsoft::WRL;

typedef InterfaceList<IMFSample>  SampleList;
typedef InterfaceList<IUnknown>   TokenList;    // List of tokens for IMFMediaStream::RequestSample

// The media stream object.
class FlvStream : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMFMediaStream, IMFMediaStreamExt>
{
    ~FlvStream();
public:
  FlvStream() = default;
  HRESULT RuntimeClassInitialize(IMFMediaSourceExt *pSource, IMFStreamDescriptor *pSD);

  // IMFMediaEventGenerator
  STDMETHODIMP   BeginGetEvent(IMFAsyncCallback* pCallback,IUnknown* punkState);
  STDMETHODIMP   EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent);
  STDMETHODIMP   GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent);
  STDMETHODIMP   QueueEvent(MediaEventType met, REFGUID mextype, HRESULT status, const PROPVARIANT* v);

  // IMFMediaStream
  STDMETHODIMP   GetMediaSource(IMFMediaSource** ppMediaSource);
  STDMETHODIMP   GetStreamDescriptor(IMFStreamDescriptor** ppStreamDescriptor);
  STDMETHODIMP   RequestSample(IUnknown* pToken);

  // Other methods (called by source)
  STDMETHODIMP   Active(BOOL act);
  STDMETHODIMP   Start(const PROPVARIANT* varStart);
  STDMETHODIMP   Pause();
  STDMETHODIMP   Stop();
  STDMETHODIMP   EndOfStream();
  STDMETHODIMP   Shutdown();
  STDMETHODIMP   IsActived()const { return activated ? S_OK : S_FALSE; }
  STDMETHODIMP   NeedsData();

  STDMETHODIMP   DeliverPayload(IMFSample* pSample);

private:
  HRESULT CheckAcceptRequestSample()const;
  HRESULT CheckShutdown() const
  {
    return (m_state == SourceState::STATE_SHUTDOWN ? MF_E_SHUTDOWN : S_OK);
  }
  HRESULT DispatchSamples();

private:
 IMFMediaSourceExt*     source;         // Parent media source, needn't add ref
 IMFStreamDescriptorPtr stream_descriptor;
 IMFMediaEventQueuePtr  event_queue;    // Event generator helper

 SourceState            m_state   = SourceState::STATE_STOPPED;
 bool                   activated = false; // Is the stream active?
 bool                   eos       = false; // Did the source reach the end of the stream?

 SampleList             samples;    // Samples waiting to be delivered.
 TokenList              requests;   // Sample requests, waiting to be dispatched.
};
