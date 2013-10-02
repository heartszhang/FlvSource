#pragma once

class FlvSource;

// The media stream object.
class FlvStream : public IMFMediaStream
{
public:

    FlvStream(FlvSource *pSource, IMFStreamDescriptor *pSD, HRESULT& hr);
    ~FlvStream();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

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

    // Callbacks
 //   HRESULT     OnDispatchSamples(IMFAsyncResult *pResult);

private:
  HRESULT CheckAcceptRequestSample()const;

private:

    HRESULT CheckShutdown() const
    {
        return ( m_state == STATE_SHUTDOWN ? MF_E_SHUTDOWN : S_OK );
    }
    HRESULT DispatchSamples();


private:
    long                m_cRef = 1;                 // reference count, 标准的Interface实现

    FlvSource         *source;             // Parent media source
    IMFStreamDescriptorPtr stream_descriptor;
    IMFMediaEventQueuePtr  event_queue;         // Event generator helper

    SourceState         m_state = STATE_STOPPED;        // Current state (running, stopped, paused)
    bool                activated = false;              // Is the stream active?
    bool                eos   = false;          // Did the source reach the end of the stream?

    SampleList          m_Samples;              // Samples waiting to be delivered.
    TokenList           m_Requests;             // Sample requests, waiting to be dispatched.
};
