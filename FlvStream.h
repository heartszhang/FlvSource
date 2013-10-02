#pragma once
#include <wrl\implements.h>
#include <mfidl.h>      //IMFMediaStream
#include <mferror.h>
#include "MFMediaSourceExt.hpp"
#include "InterfaceList.hpp"
using namespace Microsoft::WRL;

typedef InterfaceList<IMFSample>       SampleList;
typedef InterfaceList<IUnknown>  TokenList;    // List of tokens for IMFMediaStream::RequestSample

// The media stream object.
class FlvStream : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMFMediaStream, IMFMediaStreamExt>
{
    ~FlvStream();
public:
  FlvStream() = default;
  HRESULT RuntimeClassInitialize(IMFMediaSourceExt *pSource, IMFStreamDescriptor *pSD);

    // IMFMediaEventGenerator
    STDMETHODIMP BeginGetEvent(IMFAsyncCallback* pCallback,IUnknown* punkState);
    STDMETHODIMP EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent);
    STDMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent);
    STDMETHODIMP QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue);

    // IMFMediaStream
    STDMETHODIMP GetMediaSource(IMFMediaSource** ppMediaSource);
    STDMETHODIMP GetStreamDescriptor(IMFStreamDescriptor** ppStreamDescriptor);
    STDMETHODIMP RequestSample(IUnknown* pToken);

    // Other methods (called by source)
    HRESULT     Activate(bool act);
    HRESULT     Start(const PROPVARIANT* varStart);
    HRESULT     Pause();
    HRESULT     Stop();
    HRESULT     EndOfStream();
    HRESULT     Shutdown();

    bool        IsActive() const { return activated; }
    bool        NeedsData();

    HRESULT     DeliverPayload(IMFSample* pSample);

private:
  HRESULT CheckAcceptRequestSample()const;

private:

    HRESULT CheckShutdown() const
    {
      return (m_state == SourceState::STATE_SHUTDOWN ? MF_E_SHUTDOWN : S_OK);
    }
    HRESULT DispatchSamples();


private:
    long                m_cRef = 1;                 // reference count, 标准的Interface实现

    IMFMediaSourceExt         *source;             // Parent media source
    IMFStreamDescriptorPtr stream_descriptor;
    IMFMediaEventQueuePtr  event_queue;         // Event generator helper

    SourceState         m_state = SourceState::STATE_STOPPED;        // Current state (running, stopped, paused)
    bool                activated = false;              // Is the stream active?
    bool                eos   = false;          // Did the source reach the end of the stream?

    SampleList          m_Samples;              // Samples waiting to be delivered.
    TokenList           m_Requests;             // Sample requests, waiting to be dispatched.
};
