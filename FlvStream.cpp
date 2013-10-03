#include "FlvStream.h"
#include <cassert>
#include <mfapi.h>
#include "MFMediaSourceExt.hpp"

struct SourceLock {
  IMFMediaSourceExt *source;
  SourceLock(IMFMediaSourceExt *src);
  ~SourceLock();
};

//-------------------------------------------------------------------
// RequestSample:
// Requests data from the stream.
//
// pToken: Token used to track the request. Can be NULL.
//-------------------------------------------------------------------

HRESULT FlvStream::RequestSample(IUnknown* token)
{
  SourceLock lock(source);
  auto hr = CheckAcceptRequestSample();
  if (ok(hr) && eos && samples.empty())
  {
    hr = MF_E_END_OF_STREAM;
  }

  if(ok(hr))
    requests.push_back(token);

  // Dispatch the request.
  if (ok(hr))
    hr = DispatchSamples();

  if (FAILED(hr) && (m_state != SourceState::STATE_SHUTDOWN))
  {
    // An error occurred. Send an MEError even from the source,
    // unless the source is already shut down.
    hr = source->QueueEvent(MEError, GUID_NULL, hr, NULL);
  }
  return hr;
}

//selected, not shutdown and not stopped
HRESULT FlvStream::CheckAcceptRequestSample()const{
  return (m_state != SourceState::STATE_SHUTDOWN &&
          m_state != SourceState::STATE_STOPPED &&
          IsActived() == S_OK) ? S_OK : MF_E_INVALIDREQUEST;
}

HRESULT FlvStream::RuntimeClassInitialize(IMFMediaSourceExt *pSource, IMFStreamDescriptor *pSD)
{
  source = pSource;//出现互相引用的情况，所以不addref
  stream_descriptor = pSD;
  assert(pSource != NULL && pSD != NULL);

  // Create the media event queue.
  return MFCreateEventQueue(&event_queue);
}

FlvStream::~FlvStream()
{
  assert(m_state == SourceState::STATE_SHUTDOWN);
}



//-------------------------------------------------------------------
// Activate
// Activates or deactivates the stream. Called by the media source.
//-------------------------------------------------------------------

HRESULT FlvStream::Active(BOOL act)
{
    SourceLock lock(source);

    if (activated == !!act)    {
        return S_OK; // No op
    }

    activated = !!act;

    if (!act) {
      samples.clear();
      requests.clear();
    }

    return S_OK;
}


//-------------------------------------------------------------------
// Start
// Starts the stream. Called by the media source.
//
// start: Starting position. VT_EMPTY, VT_I8, not null
//-------------------------------------------------------------------

HRESULT FlvStream::Start(const PROPVARIANT* start) {
  SourceLock lock(source);

  auto hr = CheckShutdown();
  if (start && start->vt == VT_I8) {
    samples.clear();// abandon previsou cached samples, but keep tokens
  }
  // Queue the stream-started event.
  if (ok(hr)) {
    hr = QueueEvent(MEStreamStarted, GUID_NULL, S_OK, start);
  }

  if (ok(hr)) {
    m_state = SourceState::STATE_STARTED;
  }

  // If we are restarting from paused, there may be
  // queue sample requests. Dispatch them now.
  if (ok(hr)) {
    hr = DispatchSamples();
  }
  return hr;
}


//-------------------------------------------------------------------
// Pause
// Pauses the stream. Called by the media source.
//-------------------------------------------------------------------

HRESULT FlvStream::Pause()
{
  SourceLock lock(source);

  auto hr = CheckShutdown();

  if (SUCCEEDED(hr))
  {
    m_state = SourceState::STATE_PAUSED;

    hr = QueueEvent(MEStreamPaused, GUID_NULL, S_OK, NULL);
  }

  return hr;
}


//-------------------------------------------------------------------
// Stop
// Stops the stream. Called by the media source.
//-------------------------------------------------------------------

HRESULT FlvStream::Stop()
{
  SourceLock lock(source);

  auto hr = CheckShutdown();

  if (SUCCEEDED(hr))
  {
    requests.clear();
    samples.clear();

    m_state = SourceState::STATE_STOPPED;

    hr = QueueEvent(MEStreamStopped, GUID_NULL, S_OK, NULL);
  }

  return hr;
}

//-------------------------------------------------------------------
// EndOfStream
// Notifies the stream that the source reached the end of the MPEG-1
// stream. For more information, see FlvSource::EndOfMPEGStream().
//-------------------------------------------------------------------

HRESULT FlvStream::EndOfStream()
{
  SourceLock lock(source);

  eos = true;
  // will notify source eos
  return DispatchSamples();
}


//-------------------------------------------------------------------
// Shutdown
// Shuts down the stream and releases all resources.
//-------------------------------------------------------------------

HRESULT FlvStream::Shutdown()
{
  SourceLock lock(source);

  auto hr = CheckShutdown();
  if(fail(hr))
    return hr;

  m_state = SourceState::STATE_SHUTDOWN;

  // Shut down the event queue.
  if (event_queue)
  {
    event_queue->Shutdown();
  }

  // Release objects.
  samples.clear();
  requests.clear();
  event_queue = nullptr;
  stream_descriptor = nullptr;

  // NOTE:
  // Do NOT release the source pointer here, because the stream uses
  // it to hold the critical section. In particular, the stream must
  // hold the critical section when checking the shutdown status,
  // which obviously can occur after the stream is shut down.

  source = nullptr;
  return hr;
}

const DWORD SAMPLE_QUEUE = 4;               // How many samples does each stream try to hold in its queue?

//-------------------------------------------------------------------
// NeedsData
// Returns TRUE if the stream needs more data.
//-------------------------------------------------------------------

HRESULT FlvStream::NeedsData()
{
  SourceLock lock(source);

  // Note: The stream tries to keep a minimum number of samples
  // queued ahead.
  return (activated && !eos && (samples.size() < SAMPLE_QUEUE)) ? S_OK : S_FALSE;
}


//-------------------------------------------------------------------
// DeliverPayload
// Delivers a sample to the stream. called by source
//-------------------------------------------------------------------

HRESULT FlvStream::DeliverPayload(IMFSample* sample)
{
  // should we check shutdown status?
  SourceLock lock(source);

  // Queue the sample.
  samples.push_back(sample);

  // Deliver the sample if there is an outstanding request.
  return DispatchSamples();
}

//-------------------------------------------------------------------
// DispatchSamples
// Dispatches as many pending sample requests as possible.
//-------------------------------------------------------------------

HRESULT FlvStream::DispatchSamples()
{
    HRESULT hr = S_OK;

    SourceLock lock(source);

    // An I/O request can complete after the source is paused, stopped, or
    // shut down. Do not deliver samples unless the source is running.
    if (m_state != SourceState::STATE_STARTED)
    {
        return S_OK;
    }

    // Deliver as many samples as we can.
    while (ok(hr) && !samples.empty() && !requests.empty())
    {
      IMFSamplePtr sample = samples.pop_front();
      ComPtr<IUnknown> token = requests.pop_front();

        // Pull the next request token from the queue. Tokens can be NULL.
      assert(sample); // token can be null

      if (token)
        hr = sample->SetUnknown(MFSampleExtension_Token, token.Get());

      // Send an MEMediaSample event with the sample.
      if(ok(hr))
        hr = event_queue->QueueEventParamUnk(MEMediaSample, GUID_NULL, S_OK, sample.Get());
    }

    if (ok(hr) && samples.empty() && eos)
    {
        // The sample queue is empty AND we have reached the end of the source
        // stream. Notify the pipeline by sending the end-of-stream event.

        hr = event_queue->QueueEventParamVar(
            MEEndOfStream, GUID_NULL, S_OK, NULL);

        // Notify the source. It will send the end-of-presentation event.
        // hr = m_pSource->QueueAsyncOperation(SourceOp::OP_END_OF_STREAM);
        hr = source->AsyncEndOfStream();
    } else if (ok(hr) && NeedsData() == S_OK) {
        // The sample queue is empty; the request queue is not empty; and we
        // have not reached the end of the stream. Ask for more data.
      hr = source->AsyncRequestData();
    }

    if (FAILED(hr) && (m_state != SourceState::STATE_SHUTDOWN))
    {
        // An error occurred. Send an MEError even from the source,
        // unless the source is already shut down.
      source->QueueEvent(MEError, GUID_NULL, hr, NULL);
    }

    return S_OK;
}

//-------------------------------------------------------------------
// GetMediaSource:
// Returns a pointer to the media source.
//-------------------------------------------------------------------

HRESULT FlvStream::GetMediaSource(IMFMediaSource** ppMediaSource)
{
  SourceLock lock(source);

  if (ppMediaSource == NULL)
  {
    return E_POINTER;
  }
  *ppMediaSource = nullptr;
  if (source == NULL)
  {
    return E_UNEXPECTED;
  }

  HRESULT hr = CheckShutdown();

  // QI the source for IMFMediaSource.
  // (Does not hold the source's critical section.)
  if (SUCCEEDED(hr))
  {
    hr = source->QueryInterface(IID_PPV_ARGS(ppMediaSource));
  }
  return hr;
}


//-------------------------------------------------------------------
// GetStreamDescriptor:
// Returns a pointer to the stream descriptor for this stream.
//-------------------------------------------------------------------

HRESULT FlvStream::GetStreamDescriptor(IMFStreamDescriptor** ppStreamDescriptor)
{
  SourceLock lock(source);

  if (ppStreamDescriptor == NULL)
  {
    return E_POINTER;
  }
  *ppStreamDescriptor = nullptr;
  if (stream_descriptor == NULL)
  {
    return E_UNEXPECTED;
  }

  HRESULT hr = CheckShutdown();

  if (SUCCEEDED(hr))
  {
    *ppStreamDescriptor = stream_descriptor.Get();
    (*ppStreamDescriptor)->AddRef();
  };
  return hr;
}


//-------------------------------------------------------------------
// IMFMediaEventGenerator methods
//
// For remarks, see FlvSource.cpp
//-------------------------------------------------------------------

HRESULT FlvStream::BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* punkState)
{
  SourceLock lock(source);

  auto hr = CheckShutdown();

  if (SUCCEEDED(hr))
  {
    hr = event_queue->BeginGetEvent(pCallback, punkState);
  }

  return hr;
}

HRESULT FlvStream::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
  SourceLock lock(source);

  auto hr = CheckShutdown();

  if (SUCCEEDED(hr))
  {
    hr = event_queue->EndGetEvent(pResult, ppEvent);
  }

  return hr;
}

HRESULT FlvStream::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
  HRESULT hr = S_OK;

  IMFMediaEventQueuePtr pQueue;

  { // scope for lock
    SourceLock lock(source);

    // Check shutdown
    hr = CheckShutdown();

    // Cache a local pointer to the queue.
    if (SUCCEEDED(hr))
    {
      pQueue = event_queue;
    }
  }   // release lock


  // Use the local pointer to call GetEvent.
  if (SUCCEEDED(hr))
  {
    hr = pQueue->GetEvent(dwFlags, ppEvent);
  }

  return hr;
}

HRESULT FlvStream::QueueEvent(MediaEventType met, REFGUID mextype, HRESULT hrStatus, const PROPVARIANT* v)
{
  SourceLock lock(source);

  auto hr = CheckShutdown();

  if (SUCCEEDED(hr))
    hr = event_queue->QueueEventParamVar(met, mextype, hrStatus, v);

  return hr;
}

/* FlvStream::SourceLock class methods */

//-------------------------------------------------------------------
// FlvStream::SourceLock constructor - locks the source
//-------------------------------------------------------------------

SourceLock::SourceLock(IMFMediaSourceExt *src)
: source(src)
{
  if (source)
  {
    source->AddRef();
    source->Lock();
  }
}

//-------------------------------------------------------------------
// FlvStream::SourceLock destructor - unlocks the source
//-------------------------------------------------------------------

SourceLock::~SourceLock()
{
  if (source)
  {
    source->Unlock();
    source->Release();
  }
}
