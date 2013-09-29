#include "FlvSource.h"
#include "AsyncCallback.hpp"
#include "MFState.hpp"
#pragma warning( push )
#pragma warning( disable : 4355 )  // 'this' used in base member initializer list


HRESULT CreateVideoMediaType(const flv_file_header& , IMFMediaType **ppType);
HRESULT CreateAudioMediaType(const flv_file_header& , IMFMediaType **ppType);
HRESULT GetStreamMajorType(IMFStreamDescriptor *pSD, GUID *pguidMajorType);
BOOL    SampleRequestMatch(SourceOp *pOp1, SourceOp *pOp2);


//-------------------------------------------------------------------
// Name: CreateInstance
// Static method to create an instance of the source.
//
// ppSource:    Receives a ref-counted pointer to the source.
//-------------------------------------------------------------------

HRESULT FlvSource::CreateInstance(FlvSource **ppSource)
{
    if (ppSource == NULL)
    {
        return E_POINTER;
    }

    HRESULT hr = S_OK;
    FlvSource *pSource = new (std::nothrow) FlvSource(hr);
    if (pSource == NULL)
    {
        return E_OUTOFMEMORY;
    }

    if (SUCCEEDED(hr))
    {
        *ppSource = pSource;
        (*ppSource)->AddRef();
    }

    SafeRelease(&pSource);
    return hr;
}


//-------------------------------------------------------------------
// IUnknown methods
//-------------------------------------------------------------------

HRESULT FlvSource::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(FlvSource, IMFMediaEventGenerator),
        QITABENT(FlvSource, IMFMediaSource),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}

ULONG FlvSource::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG FlvSource::Release()
{
    LONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        delete this;
    }
    return cRef;
}

//-------------------------------------------------------------------
// IMFMediaEventGenerator methods
//
// All of the IMFMediaEventGenerator methods do the following:
// 1. Check for shutdown status.
// 2. Call the event queue helper object.
//-------------------------------------------------------------------

HRESULT FlvSource::BeginGetEvent(IMFAsyncCallback* pCallback,IUnknown* punkState)
{
    HRESULT hr = S_OK;

    EnterCriticalSection(&m_critSec);

    hr = CheckShutdown();

    if (SUCCEEDED(hr))
    {
        hr = m_pEventQueue->BeginGetEvent(pCallback, punkState);
    }

    LeaveCriticalSection(&m_critSec);
    return hr;
}

HRESULT FlvSource::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
    HRESULT hr = S_OK;

    EnterCriticalSection(&m_critSec);

    hr = CheckShutdown();

    if (SUCCEEDED(hr))
    {
        hr = m_pEventQueue->EndGetEvent(pResult, ppEvent);
    }

    LeaveCriticalSection(&m_critSec);
    return hr;
}

HRESULT FlvSource::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
    // NOTE:
    // GetEvent can block indefinitely, so we don't hold the critical
    // section. Therefore we need to use a local copy of the event queue
    // pointer, to make sure the pointer remains valid.

    HRESULT hr = S_OK;

    IMFMediaEventQueuePtr pQueue;

    EnterCriticalSection(&m_critSec);

    // Check shutdown
    hr = CheckShutdown();

    // Cache a local pointer to the queue.
    if (SUCCEEDED(hr))
    {
        pQueue = m_pEventQueue;
    }

    LeaveCriticalSection(&m_critSec);

    // Use the local pointer to call GetEvent.
    if (SUCCEEDED(hr))
    {
        hr = pQueue->GetEvent(dwFlags, ppEvent);
    }

    return hr;
}

HRESULT FlvSource::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue)
{
    HRESULT hr = S_OK;

    EnterCriticalSection(&m_critSec);

    hr = CheckShutdown();

    if (SUCCEEDED(hr))
    {
        hr = m_pEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
    }

    LeaveCriticalSection(&m_critSec);

    return hr;
}

//-------------------------------------------------------------------
// IMFMediaSource methods
//-------------------------------------------------------------------


//-------------------------------------------------------------------
// CreatePresentationDescriptor
// Returns a shallow copy of the source's presentation descriptor.
//-------------------------------------------------------------------

HRESULT FlvSource::CreatePresentationDescriptor(
    IMFPresentationDescriptor** ppPresentationDescriptor
    )
{
    if (ppPresentationDescriptor == NULL)
    {
        return E_POINTER;
    }

    HRESULT hr = S_OK;

    EnterCriticalSection(&m_critSec);

    // Fail if the source is shut down.
    hr = CheckShutdown();

    // Fail if the source was not initialized yet.
    if (SUCCEEDED(hr))
    {
        hr = IsInitialized();
    }

    // Do we have a valid presentation descriptor?
    if (SUCCEEDED(hr))
    {
      if (presentation_descriptor == NULL)
        {
            hr = MF_E_NOT_INITIALIZED;
        }
    }

    // Clone our presentation descriptor.
    if (SUCCEEDED(hr))
    {
      hr = presentation_descriptor->Clone(ppPresentationDescriptor);
    }

    LeaveCriticalSection(&m_critSec);
    return hr;
}


//-------------------------------------------------------------------
// GetCharacteristics
// Returns capabilities flags.
//-------------------------------------------------------------------

HRESULT FlvSource::GetCharacteristics(DWORD* pdwCharacteristics)
{
    if (pdwCharacteristics == NULL)
    {
        return E_POINTER;
    }

    HRESULT hr = S_OK;

    EnterCriticalSection(&m_critSec);

    hr = CheckShutdown();

    if (SUCCEEDED(hr))
    {
        *pdwCharacteristics =  MFMEDIASOURCE_CAN_PAUSE;
    }

    // NOTE: This sample does not implement seeking, so we do not
    // include the MFMEDIASOURCE_CAN_SEEK flag.

    LeaveCriticalSection(&m_critSec);
    return hr;
}


//-------------------------------------------------------------------
// Pause
// Pauses the source.
//-------------------------------------------------------------------

HRESULT FlvSource::Pause()
{
    EnterCriticalSection(&m_critSec);

    HRESULT hr = S_OK;

    // Fail if the source is shut down.
    hr = CheckShutdown();

    // Queue the operation.
    if (SUCCEEDED(hr))
    {
        hr = QueueAsyncOperation(SourceOp::OP_PAUSE);
    }

    LeaveCriticalSection(&m_critSec);
    return hr;
}

//-------------------------------------------------------------------
// Shutdown
// Shuts down the source and releases all resources.
//-------------------------------------------------------------------

HRESULT FlvSource::Shutdown()
{
    EnterCriticalSection(&m_critSec);

    HRESULT hr = S_OK;

    FlvStream *pStream = NULL;

    hr = CheckShutdown();

    if (SUCCEEDED(hr))
    {
        // Shut down the stream objects.
//      audio_stream->Shutdown();
//      video_stream->Shutdown();

        // Shut down the event queue.
        if (m_pEventQueue)
        {
            (void)m_pEventQueue->Shutdown();
        }

        // Release objects.

        m_pEventQueue = nullptr;
        presentation_descriptor = nullptr;
        SafeRelease(&m_pBeginOpenResult);
        byte_stream = nullptr;
        SafeRelease(&m_pCurrentOp);

        // Set the state.
        m_state = STATE_SHUTDOWN;
    }

    LeaveCriticalSection(&m_critSec);
    return hr;
}


//-------------------------------------------------------------------
// Start
// Starts or seeks the media source.
//-------------------------------------------------------------------

HRESULT FlvSource::Start(
        IMFPresentationDescriptor* pPresentationDescriptor,
        const GUID* pguidTimeFormat,
        const PROPVARIANT* pvarStartPos
    )
{

    HRESULT hr = S_OK;
    SourceOp *pAsyncOp = NULL;

    // Check parameters.

    // Start position and presentation descriptor cannot be NULL.
    if (pvarStartPos == NULL || pPresentationDescriptor == NULL)
    {
        return E_INVALIDARG;
    }

    // Check the time format.
    if ((pguidTimeFormat != NULL) && (*pguidTimeFormat != GUID_NULL))
    {
        // Unrecognized time format GUID.
        return MF_E_UNSUPPORTED_TIME_FORMAT;
    }

    // Check the data type of the start position.
    if ((pvarStartPos->vt != VT_I8) && (pvarStartPos->vt != VT_EMPTY))
    {
        return MF_E_UNSUPPORTED_TIME_FORMAT;
    }

    EnterCriticalSection(&m_critSec);

    // Check if this is a seek request. This sample does not support seeking.

    if (pvarStartPos->vt == VT_I8)
    {
        // If the current state is STOPPED, then position 0 is valid.
        // Otherwise, the start position must be VT_EMPTY (current position).

        if ((m_state != STATE_STOPPED) || (pvarStartPos->hVal.QuadPart != 0))
        {
            hr = MF_E_INVALIDREQUEST;
            goto done;
        }
    }

    // Fail if the source is shut down.
    hr = CheckShutdown();
    if (FAILED(hr))
    {
        goto done;
    }

    // Fail if the source was not initialized yet.
    hr = IsInitialized();
    if (FAILED(hr))
    {
        goto done;
    }

    // Perform a sanity check on the caller's presentation descriptor.
    hr = ValidatePresentationDescriptor(pPresentationDescriptor);
    if (FAILED(hr))
    {
        goto done;
    }

    // The operation looks OK. Complete the operation asynchronously.

    hr = SourceOp::CreateStartOp(pPresentationDescriptor, &pAsyncOp);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pAsyncOp->SetData(*pvarStartPos);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = QueueOperation(pAsyncOp);

done:
    SafeRelease(&pAsyncOp);
    LeaveCriticalSection(&m_critSec);
    return hr;
}


//-------------------------------------------------------------------
// Stop
// Stops the media source.
//-------------------------------------------------------------------

HRESULT FlvSource::Stop()
{
    EnterCriticalSection(&m_critSec);

    HRESULT hr = S_OK;

    // Fail if the source is shut down.
    hr = CheckShutdown();

    // Fail if the source was not initialized yet.
    if (SUCCEEDED(hr))
    {
        hr = IsInitialized();
    }

    // Queue the operation.
    if (SUCCEEDED(hr))
    {
        hr = QueueAsyncOperation(SourceOp::OP_STOP);
    }

    LeaveCriticalSection(&m_critSec);
    return hr;
}


//-------------------------------------------------------------------
// Public non-interface methods
//-------------------------------------------------------------------

//-------------------------------------------------------------------
// BeginOpen
// Begins reading the byte stream to initialize the source.
// Called by the byte-stream handler when it creates the source.
//
// This method is asynchronous. When the operation completes,
// the callback is invoked and the byte-stream handler calls
// EndOpen.
//
// pStream: Pointer to the byte stream for the MPEG-1 stream.
// pCB: Pointer to the byte-stream handler's callback.
// pState: State object for the async callback. (Can be NULL.)
//
// Note: The source reads enough data to find one packet header
// for each audio or video stream. This enables the source to
// create a presentation descriptor that describes the format of
// each stream. The source queues the packets that it reads during
// BeginOpen.
//-------------------------------------------------------------------

HRESULT FlvSource::BeginOpen(IMFByteStream *pStream, IMFAsyncCallback *pCB, IUnknown *pState)
{
    if (pStream == NULL || pCB == NULL)
    {
        return E_POINTER;
    }

    if (m_state != STATE_INVALID)
    {
        return MF_E_INVALIDREQUEST;
    }

    HRESULT hr = S_OK;
    DWORD dwCaps = 0;

    EnterCriticalSection(&m_critSec);

    // Cache the byte-stream pointer.
    byte_stream = pStream;

    // Validate the capabilities of the byte stream.
    // The byte stream must be readable and seekable.
    hr = pStream->GetCapabilities(&dwCaps);

    if (SUCCEEDED(hr))
    {
        if ((dwCaps & MFBYTESTREAM_IS_SEEKABLE) == 0)
        {
            hr = MF_E_BYTESTREAM_NOT_SEEKABLE;
        }
        else if ((dwCaps & MFBYTESTREAM_IS_READABLE) == 0)
        {
            hr = E_FAIL;
        }
    }

    // Create an async result object. We'll use it later to invoke the callback.
    if (SUCCEEDED(hr))
    {
        hr = MFCreateAsyncResult(NULL, pCB, pState, &m_pBeginOpenResult);
    }

    // Start reading data from the stream.
    if (SUCCEEDED(hr))
    {
      hr = ReadFlvHeader();
    }

    // At this point, we now guarantee to invoke the callback.
    if (SUCCEEDED(hr))
    {
        m_state = STATE_OPENING;
    }

    LeaveCriticalSection(&m_critSec);
    return hr;
}
HRESULT FlvSource::ReadFlvHeader() {
  auto hr = parser.begin_flv_header(byte_stream, &on_flv_header, nullptr);
  return hr;
}
HRESULT FlvSource::OnFlvHeader(IMFAsyncResult *result){
  flv_header v;
  auto hr = parser.end_flv_header(result, &v);
  if (FAILED(hr)) {
    Shutdown();
    return S_OK;
  }
  header.status.file_header_ready = 1;
  ReadFlvTagHeader();
  return S_OK;
}
HRESULT FlvSource::ReadFlvTagHeader() {
  auto hr = parser.begin_tag_header(1, &on_tag_header, nullptr);
  return hr;
}
HRESULT FlvSource::OnFlvTagHeader(IMFAsyncResult *result){
  tag_header tagh;
  auto hr = parser.end_tag_header(result, &tagh);
  if (FAILED(hr) || FAILED(result->GetStatus())){
    Shutdown();
    return S_OK;
  }
//  header.tags.push_back(tagh);
  if (tagh.type == flv::tag_type::script_data){
    header.status.has_script_data = 1;
    ReadMetaData(tagh.data_size);
  }
  else if (tagh.type == flv::tag_type::video){
    header.status.has_video = 1;
    if (!header.video)
      header.video.reset(new video_stream_header(tagh));
    if (!header.first_media_tag_offset)
      header.first_media_tag_offset = tagh.data_offset - flv::flv_tag_header_length;
    SeekToNextTag(tagh);
  }
  else if (tagh.type == flv::tag_type::audio){
    header.status.has_audio = 1;
    if(!header.audio)
      header.audio.reset(new audio_stream_header(tagh));
    if (!header.first_media_tag_offset)
      header.first_media_tag_offset = tagh.data_offset - flv::flv_tag_header_length;
    SeekToNextTag(tagh);
  }
  else if (tagh.type == flv::tag_type::eof){  // first round scan done
    header.status.scan_once = 1;
    FinishInitialize();
    // openning can be finished here
  }
  else {
    SeekToNextTag(tagh);// ignore unknown tags
  }
  return S_OK;
}
HRESULT FlvSource::SeekToNextTag(tag_header const&tagh) {
  //return // parser.begin_seek(tagh.data_size, 1, &on_seek_to_tag_begin, nullptr);
  QWORD pos = -1;
  return byte_stream->Seek(MFBYTESTREAM_SEEK_ORIGIN::msoCurrent, tagh.data_size, MFBYTESTREAM_SEEK_FLAG_CANCEL_PENDING_IO, &pos);
}

HRESULT FlvSource::ReadMetaData(uint32_t meta_size) {
  return parser.begin_on_meta_data(meta_size, &on_meta_data, nullptr);
}

HRESULT FlvSource::OnMetaData(IMFAsyncResult *result){
  auto hr = parser.end_on_meta_data(result, &header);
  if (FAILED(hr) || FAILED(result->GetStatus())){
    Shutdown();
    return S_OK;
  }
  header.status.meta_ready = 1;
//  ReadFlvTagHeader();
  FinishInitialize();
  return S_OK;
}
//-------------------------------------------------------------------
// EndOpen
// Completes the BeginOpen operation.
// Called by the byte-stream handler when it creates the source.
//-------------------------------------------------------------------

HRESULT FlvSource::EndOpen(IMFAsyncResult *pResult)
{
    EnterCriticalSection(&m_critSec);

    HRESULT hr = S_OK;

    hr = pResult->GetStatus();

    if (FAILED(hr))
    {
        // The source is not designed to recover after failing to open.
        // Switch to shut-down state.
        Shutdown();
    }
    LeaveCriticalSection(&m_critSec);
    return hr;
}

//-------------------------------------------------------------------
// OnByteStreamRead
// Called when an asynchronous read completes.
//
// Read requests are issued in the RequestData() method.
//-------------------------------------------------------------------
/*
HRESULT FlvSource::OnByteStreamRead(IMFAsyncResult *pResult)
{
    EnterCriticalSection(&m_critSec);

    HRESULT hr = S_OK;
    DWORD cbRead = 0;

    IUnknown *pState = NULL;

    if (m_state == STATE_SHUTDOWN)
    {
        // If we are shut down, then we've already released the
        // byte stream. Nothing to do.
        LeaveCriticalSection(&m_critSec);
        return S_OK;
    }

    // Get the state object. This is either NULL or the most
    // recent OP_REQUEST_DATA operation.
    (void)pResult->GetState(&pState);

    // Complete the read opertation.
    hr = m_pByteStream->EndRead(pResult, &cbRead);

    // If the source stops and restarts in rapid succession, there is
    // a chance this is a "stale" read request, initiated before the
    // stop/restart.

    // To ensure that we don't deliver stale data, we store the
    // OP_REQUEST_DATA operation as a state object in pResult, and compare
    // this against the current value of m_cRestartCounter.

    // If they don't match, we discard the data.

    // NOTE: During BeginOpen, pState is NULL

    if (SUCCEEDED(hr))
    {
        if ((pState == NULL) || ( ((SourceOp*)pState)->Data().ulVal == m_cRestartCounter) )
        {
            // This data is OK to parse.

            if (cbRead == 0)
            {
                // There is no more data in the stream. Signal end-of-stream.
                hr = EndOfMPEGStream();
            }
            else
            {
                // Update the end-position of the read buffer.
                hr = m_ReadBuffer.MoveEnd(cbRead);

                // Parse the new data.
                if (SUCCEEDED(hr))
                {
                    hr = ParseData();
                }
            }
        }
    }

    if (FAILED(hr))
    {
        StreamingError(hr);
    }
    SafeRelease(&pState);
    LeaveCriticalSection(&m_critSec);
    return hr;
}
*/

/* Private methods */

FlvSource::FlvSource(HRESULT& hr) :
    OpQueue(m_critSec),
    m_cRef(1),
    m_pEventQueue(NULL),
    presentation_descriptor(NULL),
    m_pBeginOpenResult(NULL),
    byte_stream(NULL),
    m_state(STATE_INVALID),
    m_pCurrentOp(NULL),
//    m_pSampleRequest(NULL),
    m_cRestartCounter(0),
    on_flv_header(this, &FlvSource::OnFlvHeader),
    on_tag_header(this, &FlvSource::OnFlvTagHeader),
//    on_seek_to_next_tag(this, &FlvSource::OnSeekToNextTag),
    on_meta_data(this, &FlvSource::OnMetaData),
    on_demux_sample_header(this, &FlvSource::OnSampleHeader),
    on_audio_header(this, &FlvSource::OnAudioHeader),
    on_aac_packet_type(this, &FlvSource::OnAacPacketType),
    on_audio_data(this, &FlvSource::OnAudioData),
    on_avc_packet_type(this, &FlvSource::OnAacPacketType),
    on_video_data(this, &FlvSource::OnVideoData),
    on_video_header(this, &FlvSource::OnVideoHeader)
{
    DllAddRef();

    InitializeCriticalSection(&m_critSec);

    // Create the media event queue.
    hr = MFCreateEventQueue(&m_pEventQueue);
}

FlvSource::~FlvSource()
{
    if (m_state != STATE_SHUTDOWN)
    {
        Shutdown();
    }

    DeleteCriticalSection(&m_critSec);

    DllRelease();
}


//-------------------------------------------------------------------
// CompleteOpen
//
// Completes the asynchronous BeginOpen operation.
//
// hrStatus: Status of the BeginOpen operation.
//-------------------------------------------------------------------

HRESULT FlvSource::CompleteOpen(HRESULT hrStatus)
{
    HRESULT hr = S_OK;

    if (m_pBeginOpenResult)
    {
        hr = m_pBeginOpenResult->SetStatus(hrStatus);
        if (SUCCEEDED(hr))
        {
            hr = MFInvokeCallback(m_pBeginOpenResult);
        }
    }

    SafeRelease(&m_pBeginOpenResult);
    return hr;
}


//-------------------------------------------------------------------
// IsInitialized:
// Returns S_OK if the source is correctly initialized with an
// MPEG-1 byte stream. Otherwise, returns MF_E_NOT_INITIALIZED.
//-------------------------------------------------------------------

HRESULT FlvSource::IsInitialized() const
{
    if (m_state == STATE_OPENING || m_state == STATE_INVALID)
    {
        return MF_E_NOT_INITIALIZED;
    }
    else
    {
        return S_OK;
    }
}

//-------------------------------------------------------------------
// InitPresentationDescriptor
//
// Creates the source's presentation descriptor, if possible.
//
// During the BeginOpen operation, the source reads packets looking
// for headers for each stream. This enables the source to create the
// presentation descriptor, which describes the stream formats.
//
// This method tests whether the source has seen enough packets
// to create the PD. If so, it invokes the callback to complete
// the BeginOpen operation.
//-------------------------------------------------------------------

HRESULT FlvSource::InitPresentationDescriptor()
{
    HRESULT hr = S_OK;

    assert(!presentation_descriptor);
    assert(m_state == STATE_OPENING);
    DWORD cStreams = 0;
    if (video_stream)
      ++cStreams;
    if (audio_stream)
      ++cStreams;
    // Ready to create the presentation descriptor.

    // Create an array of IMFStreamDescriptor pointers.
    IMFStreamDescriptor **ppSD =
            new (std::nothrow) IMFStreamDescriptor*[cStreams];

    ZeroMemory(ppSD, cStreams * sizeof(IMFStreamDescriptor*));

    cStreams = 0;
    if (video_stream)
      video_stream->GetStreamDescriptor(&ppSD[cStreams++]);
    if (audio_stream)
      audio_stream->GetStreamDescriptor(&ppSD[cStreams++]);

    // Create the presentation descriptor.
    hr = MFCreatePresentationDescriptor(cStreams, ppSD,      &presentation_descriptor);

    if (FAILED(hr))
    {
        goto done;
    }

    // Select the first video stream (if any).
    for (DWORD i = 0; i < cStreams; i++)
    {
      hr = presentation_descriptor->SelectStream(i);
    }

    // Switch state from "opening" to stopped.
    m_state = STATE_STOPPED;

    // Invoke the async callback to complete the BeginOpen operation.
    hr = CompleteOpen(S_OK);

done:
    // clean up:
    if (ppSD)
    {
        for (DWORD i = 0; i < cStreams; i++)
        {
            SafeRelease(&ppSD[i]);
        }
        delete [] ppSD;
    }
    return hr;
}


//-------------------------------------------------------------------
// QueueAsyncOperation
// Queue an asynchronous operation.
//
// OpType: Type of operation to queue.
//
// Note: If the SourceOp object requires additional information, call
// OpQueue<SourceOp>::QueueOperation, which takes a SourceOp pointer.
//-------------------------------------------------------------------

HRESULT FlvSource::QueueAsyncOperation(SourceOp::Operation OpType)
{
    HRESULT hr = S_OK;
    SourceOp *pOp = NULL;

    hr = SourceOp::CreateOp(OpType, &pOp);

    if (SUCCEEDED(hr))
    {
        hr = QueueOperation(pOp);
    }

    SafeRelease(&pOp);
    return hr;
}

//-------------------------------------------------------------------
// BeginAsyncOp
//
// Starts an asynchronous operation. Called by the source at the
// begining of any asynchronous operation.
//-------------------------------------------------------------------

HRESULT FlvSource::BeginAsyncOp(SourceOp *pOp)
{
    // At this point, the current operation should be NULL (the
    // previous operation is NULL) and the new operation (pOp)
    // must not be NULL.

    if (pOp == NULL || m_pCurrentOp != NULL)
    {
        assert(FALSE);
        return E_FAIL;
    }

    // Store the new operation as the current operation.

    m_pCurrentOp = pOp;
    m_pCurrentOp->AddRef();

    return S_OK;
}

//-------------------------------------------------------------------
// CompleteAsyncOp
//
// Completes an asynchronous operation. Called by the source at the
// end of any asynchronous operation.
//-------------------------------------------------------------------

HRESULT FlvSource::CompleteAsyncOp(SourceOp *pOp)
{
    HRESULT hr = S_OK;

    // At this point, the current operation (m_pCurrentOp)
    // must match the operation that is ending (pOp).

    if (pOp == NULL || m_pCurrentOp == NULL)
    {
        assert(FALSE);
        return E_FAIL;
    }

    if (m_pCurrentOp != pOp)
    {
        assert(FALSE);
        return E_FAIL;
    }

    // Release the current operation.
    SafeRelease(&m_pCurrentOp);

    // Process the next operation on the queue.
    hr = ProcessQueue();

    return hr;
}

//-------------------------------------------------------------------
// DispatchOperation
//
// Performs the asynchronous operation indicated by pOp.
//
// NOTE:
// This method implements the pure-virtual OpQueue::DispatchOperation
// method. It is always called from a work-queue thread.
//-------------------------------------------------------------------

HRESULT FlvSource::DispatchOperation(SourceOp *pOp)
{
    EnterCriticalSection(&m_critSec);

    HRESULT hr = S_OK;

    if (m_state == STATE_SHUTDOWN)
    {
        LeaveCriticalSection(&m_critSec);

        return S_OK; // Already shut down, ignore the request.
    }

    switch (pOp->Op())
    {

    // IMFMediaSource methods:

    case SourceOp::OP_START:
        hr = DoStart((StartOp*)pOp);
        break;

    case SourceOp::OP_STOP:
        hr = DoStop(pOp);
        break;

    case SourceOp::OP_PAUSE:
        hr = DoPause(pOp);
        break;

    // Operations requested by the streams:

    case SourceOp::OP_REQUEST_DATA:
        hr = OnStreamRequestSample(pOp);
        break;

    case SourceOp::OP_END_OF_STREAM:
        hr = OnEndOfStream(pOp);
        break;

    default:
        hr = E_UNEXPECTED;
    }

    if (FAILED(hr))
    {
        StreamingError(hr);
    }

    LeaveCriticalSection(&m_critSec);
    return hr;
}


//-------------------------------------------------------------------
// ValidateOperation
//
// Checks whether the source can perform the operation indicated
// by pOp at this time.
//
// If the source cannot perform the operation now, the method
// returns MF_E_NOTACCEPTING.
//
// NOTE:
// Implements the pure-virtual OpQueue::ValidateOperation method.
//-------------------------------------------------------------------

HRESULT FlvSource::ValidateOperation(SourceOp *pOp)
{
    if (m_pCurrentOp != NULL)
    {
        return MF_E_NOTACCEPTING;
    }
    return S_OK;
}



//-------------------------------------------------------------------
// DoStart
// Perform an async start operation (IMFMediaSource::Start)
//
// pOp: Contains the start parameters.
//
// Note: This sample currently does not implement seeking, and the
// Start() method fails if the caller requests a seek.
//-------------------------------------------------------------------

HRESULT FlvSource::DoStart(StartOp *pOp)
{
    assert(pOp->Op() == SourceOp::OP_START);

    IMFPresentationDescriptor *pPD = NULL;
    IMFMediaEvent  *pEvent = NULL;

    HRESULT     hr = S_OK;
    LONGLONG    llStartOffset = 0;
    BOOL        bRestartFromCurrentPosition = FALSE;
    BOOL        bSentEvents = FALSE;

    hr = BeginAsyncOp(pOp);

    // Get the presentation descriptor from the SourceOp object.
    // This is the PD that the caller passed into the Start() method.
    // The PD has already been validated.
    if (SUCCEEDED(hr))
    {
        hr = pOp->GetPresentationDescriptor(&pPD);
    }
    // Because this sample does not support seeking, the start
    // position must be 0 (from stopped) or "current position."

    // If the sample supported seeking, we would need to get the
    // start position from the PROPVARIANT data contained in pOp.

    if (SUCCEEDED(hr))
    {
        // Select/deselect streams, based on what the caller set in the PD.
        // This method also sends the MENewStream/MEUpdatedStream events.
        hr = SelectStreams(pPD, pOp->Data());
    }

    if (SUCCEEDED(hr))
    {
        m_state = STATE_STARTED;

        // Queue the "started" event. The event data is the start position.
        hr = m_pEventQueue->QueueEventParamVar(
            MESourceStarted,
            GUID_NULL,
            S_OK,
            &pOp->Data()
            );
    }

    if (FAILED(hr))
    {
        // Failure. Send the error code to the application.

        // Note: It's possible that QueueEvent itself failed, in which case it
        // is likely to fail again. But there is no good way to recover in
        // that case.

        (void)m_pEventQueue->QueueEventParamVar(
            MESourceStarted, GUID_NULL, hr, NULL);
    }

    CompleteAsyncOp(pOp);

    SafeRelease(&pEvent);
    SafeRelease(&pPD);
    return hr;
}

//-------------------------------------------------------------------
// SelectStreams
// Called during START operations to select and deselect streams.
// This method also sends the MENewStream/MEUpdatedStream events.
//-------------------------------------------------------------------
HRESULT     FlvSource::SelectStreams(IMFPresentationDescriptor *pPD, const PROPVARIANT varStart){
  HRESULT hr = S_OK;
  BOOL    fWasSelected = FALSE;

  // Reset the pending EOS count.
  m_cPendingEOS = 0;
  DWORD stream_count = 0;
  pPD->GetStreamDescriptorCount(&stream_count);
  // Loop throught the stream descriptors to find which streams are active.
  for (DWORD i = 0; i < stream_count; i++)
  {
    IMFStreamDescriptorPtr pSD = NULL;
    BOOL    fSelected = FALSE;
    DWORD stream_id = 0 - 1ul;
    hr = pPD->GetStreamDescriptorByIndex(i, &fSelected, &pSD);
    if (FAILED(hr))
    {
      goto done;
    }

    hr = pSD->GetStreamIdentifier(&stream_id);
    if (FAILED(hr))
    {
      goto done;
    }
    IMFMediaStreamPtr stream;
    if (stream_id == 1){
      stream = audio_stream;
    }
    else if (stream_id = 0){
      stream = video_stream;
    }
    if (!stream)
    {
      hr = E_INVALIDARG;
      goto done;
    }
    FlvStream* flv_stream = static_cast<FlvStream*>(stream.GetInterfacePtr());
    // Was the stream active already?
    fWasSelected = flv_stream->IsActive();

    // Activate or deactivate the stream.
    hr = flv_stream->Activate(!!fSelected);
    if (FAILED(hr))
    {
      goto done;
    }

    if (fSelected)
    {
      m_cPendingEOS++;

      // If the stream was previously selected, send an "updated stream"
      // event. Otherwise, send a "new stream" event.
      MediaEventType met = fWasSelected ? MEUpdatedStream : MENewStream;

      hr = m_pEventQueue->QueueEventParamUnk(met, GUID_NULL, hr, stream);
      if (FAILED(hr))
      {
        goto done;
      }

      // Start the stream. The stream will send the appropriate event.
      hr = flv_stream->Start(varStart);
      if (FAILED(hr))
      {
        goto done;
      }
    }
    else if (fWasSelected){
      flv_stream->Shutdown();
    }
  }
done:
  return hr;
}

//-------------------------------------------------------------------
// DoStop
// Perform an async stop operation (IMFMediaSource::Stop)
//-------------------------------------------------------------------

HRESULT FlvSource::DoStop(SourceOp *pOp)
{
    HRESULT hr = S_OK;
    QWORD qwCurrentPosition = 0;

    hr = BeginAsyncOp(pOp);

    // Stop the active streams.


    // Seek to the start of the file. If we restart after stopping,
    // we will start from the beginning of the file again.
    if (SUCCEEDED(hr))
    {
        hr = byte_stream->Seek(
            msoBegin,
            0,
            MFBYTESTREAM_SEEK_FLAG_CANCEL_PENDING_IO,
            &qwCurrentPosition
            );
    }

    // Increment the counter that tracks "stale" read requests.
    if (SUCCEEDED(hr))
    {
        ++m_cRestartCounter; // This counter is allowed to overflow.
    }

//    SafeRelease(&m_pSampleRequest);
    status.pending_request = 1;
    m_state = STATE_STOPPED;

    // Send the "stopped" event. This might include a failure code.
    (void)m_pEventQueue->QueueEventParamVar(MESourceStopped, GUID_NULL, hr, NULL);

    CompleteAsyncOp(pOp);

    return hr;
}


//-------------------------------------------------------------------
// DoPause
// Perform an async pause operation (IMFMediaSource::Pause)
//-------------------------------------------------------------------

HRESULT FlvSource::DoPause(SourceOp *pOp)
{
    HRESULT hr = S_OK;

    hr = BeginAsyncOp(pOp);

    // Pause is only allowed while running.
    if (SUCCEEDED(hr))
    {
        if (m_state != STATE_STARTED)
        {
            hr = MF_E_INVALID_STATE_TRANSITION;
        }
    }

    // Pause the active streams.
    if (SUCCEEDED(hr))
    {
    }

    m_state = STATE_PAUSED;


    // Send the "paused" event. This might include a failure code.
    (void)m_pEventQueue->QueueEventParamVar(MESourcePaused, GUID_NULL, hr, NULL);

    CompleteAsyncOp(pOp);

    return hr;
}


//-------------------------------------------------------------------
// StreamRequestSample
// Called by streams when they need more data.
//
// Note: This is an async operation. The stream requests more data
// by queueing an OP_REQUEST_DATA operation.
//-------------------------------------------------------------------

HRESULT FlvSource::OnStreamRequestSample(SourceOp *pOp)
{
    HRESULT hr = S_OK;
    DemuxSample();
    hr = CompleteAsyncOp(pOp);
    return hr;
}

void FlvSource::DemuxSample(){
  if (!NeedDemux())
    return;
  status.pending_request = 1;
  ReadSampleHeader();
}
HRESULT FlvSource::ReadSampleHeader(){
  auto hr= parser.begin_tag_header(0, &on_demux_sample_header, nullptr);
  if (fail(hr)){
    Shutdown();
  }
  return hr;
}
HRESULT FlvSource::OnSampleHeader(IMFAsyncResult *result){
  tag_header tagh;
  auto hr = parser.end_tag_header(result, &tagh);
  if (tagh.type == flv::tag_type::eof){
    hr = EndOfFile();
  }
  else if (tagh.type == flv::tag_type::audio){
    hr = ReadAudioHeader(tagh);
  }
  else if (tagh.type == flv::tag_type::video){
    hr = ReadVideoHeader(tagh);
  }
  else if (fail(hr)){
    hr = Shutdown();
  }
  else {
    hr = SeekToNextTag(tagh);
  }
  return hr;
}
HRESULT FlvSource::ReadVideoHeader(tag_header const&h){
  auto hr = parser.begin_video_header(&on_video_header, NewMFState(h));
  if (fail(hr))
    Shutdown();
  return hr;
}

HRESULT FlvSource::OnVideoHeader(IMFAsyncResult*result){
  video_header vh;
  auto hr = parser.end_video_header(result, &vh);
  video_stream_header vsh(FromAsyncResultState<tag_header>(result), vh);
  if (ok(hr)){
    if (vsh.codec_id == flv::video_codec::avc) {
      ReadAvcPacketType(vsh);
    }
    else{
      ReadVideoData(vsh);
    }
  }
  if (fail(hr))
    Shutdown();
  return hr;
}

HRESULT FlvSource::ReadAudioHeader(tag_header const&h){
  auto hr = parser.begin_audio_header(&on_audio_header, NewMFState(h));
  if (fail(hr))
    Shutdown();
  return hr;
}
HRESULT FlvSource::OnAudioHeader(IMFAsyncResult* result){
  audio_header ah;
  auto hr = parser.end_audio_header(result, &ah);
  tag_header const&th = FromAsyncResultState<tag_header>(result);
  audio_stream_header ash(th, ah);
  if (ok(hr)){
    if (ah.codec_id == flv::audio_codec::aac)
      ReadAacPacketType(ash); // ignore return value
    else ReadAudioData(ash);
  }
  if (fail(hr)){
    Shutdown();
  }
  return hr;
}

HRESULT FlvSource::ReadAacPacketType(audio_stream_header const&ash){
  auto hr = parser.begin_aac_packet_type(&on_aac_packet_type, NewMFState(ash));
  if (fail(hr))
    Shutdown();
  return hr;
}
HRESULT FlvSource::OnAacPacketType(IMFAsyncResult*result){
  auto &ash = FromAsyncResultState<audio_stream_header>(result);
  auto hr = parser.end_aac_packet_type(result, &ash.aac_packet_type);
  if (ok(hr)) 
    hr = byte_stream->GetCurrentPosition(&ash.data_offset);
  if (ok(hr))
    ReadAudioData(ash);
  if (fail(hr))
    Shutdown();
  return hr;
}
HRESULT FlvSource::ReadAvcPacketType(video_stream_header const&vsh){
  auto hr = parser.begin_avc_header(&on_avc_packet_type, NewMFState(vsh));
  if (fail(hr))
    Shutdown();
  return hr;
}
HRESULT FlvSource::OnAvcPacketType(IMFAsyncResult*result){
  avc_header v;
  auto hr = parser.end_avc_header(result, &v);
  auto &vsh = FromAsyncResultState<video_stream_header>(result);
  vsh.avc_packet_type = v.avc_packet_type;
  vsh.composition_time = v.composite_time;
  if (ok(hr))
    ReadVideoData(vsh);
  if (fail(hr))
    Shutdown();
  return hr;
}
HRESULT FlvSource::ReadAudioData(audio_stream_header const& ash){
  auto hr = parser.begin_audio_data(ash.payload_length(), &on_audio_data, NewMFState<audio_stream_header>(ash));
  if (fail(hr))
    Shutdown();
  return hr;
}
HRESULT FlvSource::OnAudioData(IMFAsyncResult *result){
  auto &ash = FromAsyncResultState<audio_stream_header>(result);
  packet pack;
  auto  hr = parser.end_audio_data(result, &pack);
  IMFMediaBufferPtr mbuf;
  hr = MFCreateMemoryBuffer(pack.length, &mbuf);

  BYTE*pb = nullptr;
  mbuf->Lock(&pb, nullptr, nullptr);
  memcpy(pb, pack._, pack.length);
  mbuf->SetCurrentLength(pack.length);
  mbuf->Unlock();
  IMFSamplePtr sample;
  if (ok(hr))
    hr = MFCreateSample(&sample);
  sample->AddBuffer(mbuf);
  sample->SetSampleTime(ash.nano_time());
  if (ok(hr)){
    auto astream = static_cast<FlvStream*>(audio_stream.GetInterfacePtr());
    hr = astream->DeliverPayload(sample);
  }
  status.pending_request = 0;
  if (fail(hr))
    Shutdown();
  DemuxSample();
  return hr;
}
HRESULT FlvSource::ReadVideoData(video_stream_header const& vsh){  
  auto hr = parser.begin_video_data(vsh.payload_length(), &on_video_data, NewMFState<video_stream_header>(vsh));
  if (fail(hr))
    Shutdown();
  return hr;
}
HRESULT FlvSource::OnVideoData(IMFAsyncResult *result){
  auto &ash = FromAsyncResultState<video_stream_header>(result);
  packet pack;
  auto  hr = parser.end_video_data(result, &pack);

  IMFMediaBufferPtr mbuf;
  hr = MFCreateMemoryBuffer(pack.length, &mbuf);
  BYTE* pb = nullptr;
  mbuf->Lock(&pb, nullptr, nullptr);
  memcpy(pb, pack._, pack.length);
  mbuf->SetCurrentLength(pack.length);
  mbuf->Unlock();
  IMFSamplePtr sample;
  if (ok(hr))
    hr = MFCreateSample(&sample);
  sample->AddBuffer(mbuf);
  sample->SetSampleTime(ash.nano_time());
  sample->SetUINT32(MFSampleExtension_CleanPoint, ash.frame_type == flv::frame_type::key_frame ? 1 : 0);
  // should set sample duration

  if (ok(hr)){
    auto astream = static_cast<FlvStream*>(video_stream.GetInterfacePtr());
    hr = astream->DeliverPayload(sample);
  }
  status.pending_request = 0;
  if (fail(hr))
    Shutdown();
  DemuxSample();
  return hr;
}

HRESULT FlvSource::EndOfFile(){
  auto vstream = static_cast<FlvStream*>(video_stream.GetInterfacePtr());
  auto astream = static_cast<FlvStream*>(audio_stream.GetInterfacePtr());
  if (vstream)
    vstream->EndOfStream();
  if (astream)
    astream->EndOfStream();
  return S_OK;
}
bool FlvSource::NeedDemux() {
  if (fail(CheckShutdown())){
    return false;
  }
  if (status.pending_request)
    return false;
  auto vstream = static_cast<FlvStream*>(video_stream.GetInterfacePtr());
  auto astream = static_cast<FlvStream*>(audio_stream.GetInterfacePtr());
  if (vstream && vstream->NeedsData())
    return true;
  if (astream && astream->NeedsData())
    return true;
  return false;
}
//-------------------------------------------------------------------
// OnEndOfStream
// Called by each stream when it sends the last sample in the stream.
//
// Note: When the media source reaches the end of the MPEG-1 stream,
// it calls EndOfStream on each stream object. The streams might have
// data still in their queues. As each stream empties its queue, it
// notifies the source through an async OP_END_OF_STREAM operation.
//
// When every stream notifies the source, the source can send the
// "end-of-presentation" event.
//-------------------------------------------------------------------

HRESULT FlvSource::OnEndOfStream(SourceOp *pOp)
{
    HRESULT hr = S_OK;

    hr = BeginAsyncOp(pOp);

    // Decrement the count of end-of-stream notifications.
    if (SUCCEEDED(hr))
    {
        --m_cPendingEOS;
        if (m_cPendingEOS == 0)
        {
            // No more streams. Send the end-of-presentation event.
            hr = m_pEventQueue->QueueEventParamVar(MEEndOfPresentation, GUID_NULL, S_OK, NULL);
        }

    }

    if (SUCCEEDED(hr))
    {
        hr = CompleteAsyncOp(pOp);
    }

    return hr;
}

HRESULT FlvSource::FinishInitialize(){
  HRESULT hr = S_OK;
  if (header.first_media_tag_offset) {
    hr = byte_stream->SetCurrentPosition(header.first_media_tag_offset);
  }
  CreateAudioStream();
  CreateVideoStream();
  hr = InitPresentationDescriptor();

  return hr;
}

HRESULT FlvSource::CreateStream(DWORD index, IMFMediaType*media_type, IMFMediaStream**v) {
  _com_ptr_t < _com_IIID<IMFMediaType, &__uuidof(IMFMediaType) >> pType;
  _com_ptr_t < _com_IIID < IMFStreamDescriptor, &__uuidof(IMFStreamDescriptor)>> pSD;
  _com_ptr_t < _com_IIID < IMFMediaTypeHandler, &__uuidof(IMFMediaTypeHandler)>> pHandler;

//  auto hr = CreateAudioMediaType(header, &pType);
  // Create the stream descriptor from the media type.
  auto  hr = MFCreateStreamDescriptor(index, 1, &media_type, &pSD);

  // Set the default media type on the stream handler.
  if (SUCCEEDED(hr))
  {
    hr = pSD->GetMediaTypeHandler(&pHandler);
  }
  if (SUCCEEDED(hr))
  {
    hr = pHandler->SetCurrentMediaType(pType);
  }

  // Create the new stream.
  if (SUCCEEDED(hr))
  {
    *v = new (std::nothrow) FlvStream(this, pSD, hr);
  }

  // Add the stream to the array.

  return hr;
}
//-------------------------------------------------------------------
// CreateStream:
// Creates a media stream, based on a packet header.
//-------------------------------------------------------------------

HRESULT FlvSource::CreateAudioStream()
{
  IMFMediaTypePtr media_type;
  auto hr = CreateAudioMediaType(header, &media_type);
  if (ok(hr))  // audio stream_index  is 1
    hr = CreateStream(1, media_type.GetInterfacePtr(), &audio_stream);
  return hr;
}

HRESULT FlvSource::CreateVideoStream(){
  IMFMediaTypePtr media_type;
  auto hr = CreateVideoMediaType(header, &media_type);
  if (ok(hr))
    hr = CreateStream(0, media_type.GetInterfacePtr(), &video_stream);
  return hr;
}

//-------------------------------------------------------------------
// ValidatePresentationDescriptor:
// Validates the presentation descriptor that the caller specifies
// in IMFMediaSource::Start().
//
// Note: This method performs a basic sanity check on the PD. It is
// not intended to be a thorough validation.
//-------------------------------------------------------------------

HRESULT FlvSource::ValidatePresentationDescriptor(IMFPresentationDescriptor *pPD)
{
    HRESULT hr = S_OK;
    DWORD cStreams = 0;

//    IMFStreamDescriptor *pSD = NULL;

    // The caller's PD must have the same number of streams as ours.
    //hr = pPD->GetStreamDescriptorCount(&cStreams);

    // The caller must select at least one stream.
    return hr;
}


//-------------------------------------------------------------------
// StreamingError:
// Handles an error that occurs duing an asynchronous operation.
//
// hr: Error code of the operation that failed.
//-------------------------------------------------------------------

void FlvSource::StreamingError(HRESULT hr)
{
    if (m_state == STATE_OPENING)
    {
        // An error happened during BeginOpen.
        // Invoke the callback with the status code.

        CompleteOpen(hr);
    }
    else if (m_state != STATE_SHUTDOWN)
    {
        // An error occurred during streaming. Send the MEError event
        // to notify the pipeline.

        QueueEvent(MEError, GUID_NULL, hr, NULL);
    }
}



/* SourceOp class */


//-------------------------------------------------------------------
// CreateOp
// Static method to create a SourceOp instance.
//
// op: Specifies the async operation.
// ppOp: Receives a pointer to the SourceOp object.
//-------------------------------------------------------------------

HRESULT SourceOp::CreateOp(SourceOp::Operation op, SourceOp **ppOp)
{
    if (ppOp == NULL)
    {
        return E_POINTER;
    }

    SourceOp *pOp = new (std::nothrow) SourceOp(op);
    if (pOp  == NULL)
    {
        return E_OUTOFMEMORY;
    }
    *ppOp = pOp;

    return S_OK;
}

//-------------------------------------------------------------------
// CreateStartOp:
// Static method to create a SourceOp instance for the Start()
// operation.
//
// pPD: Presentation descriptor from the caller.
// ppOp: Receives a pointer to the SourceOp object.
//-------------------------------------------------------------------

HRESULT SourceOp::CreateStartOp(IMFPresentationDescriptor *pPD, SourceOp **ppOp)
{
    if (ppOp == NULL)
    {
        return E_POINTER;
    }

    SourceOp *pOp = new (std::nothrow) StartOp(pPD);
    if (pOp == NULL)
    {
        return E_OUTOFMEMORY;
    }

    *ppOp = pOp;
    return S_OK;
}


ULONG SourceOp::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG SourceOp::Release()
{
    LONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        delete this;
    }
    return cRef;
}

HRESULT SourceOp::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(SourceOp, IUnknown),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}

SourceOp::SourceOp(Operation op) : m_cRef(1), m_op(op)
{
    PropVariantInit(&m_data);
}

SourceOp::~SourceOp()
{
    PropVariantClear(&m_data);
}

HRESULT SourceOp::SetData(const PROPVARIANT& var)
{
    return PropVariantCopy(&m_data, &var);
}


StartOp::StartOp(IMFPresentationDescriptor *pPD) : SourceOp(SourceOp::OP_START), m_pPD(pPD)
{
    if (m_pPD)
    {
        m_pPD->AddRef();
    }
}

StartOp::~StartOp()
{
    SafeRelease(&m_pPD);
}


HRESULT StartOp::GetPresentationDescriptor(IMFPresentationDescriptor **ppPD)
{
    if (ppPD == NULL)
    {
        return E_POINTER;
    }
    if (m_pPD == NULL)
    {
        return MF_E_INVALIDREQUEST;
    }
    *ppPD = m_pPD;
    (*ppPD)->AddRef();
    return S_OK;
}


/*  Static functions */


//-------------------------------------------------------------------
// CreateVideoMediaType:
// Create a media type from an MPEG-1 video sequence header.
//-------------------------------------------------------------------

HRESULT CreateVideoMediaType(const flv_file_header& header, IMFMediaType **ppType)
{
    HRESULT hr = S_OK;

    IMFMediaType *pType = NULL;

    hr = MFCreateMediaType(&pType);

    if (SUCCEEDED(hr))
    {
        hr = pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    }

    // MEDIASUBTYPE_AVC1?
    if (SUCCEEDED(hr))
    {
      hr = pType->SetGUID(MF_MT_SUBTYPE, MEDIASUBTYPE_H264);
    }

    // Format details.
    if (SUCCEEDED(hr))
    {
        // Frame size

        hr = MFSetAttributeSize(
            pType,
            MF_MT_FRAME_SIZE,
            header.width,
            header.height
            );
    }
    if (SUCCEEDED(hr))
    {
        // Frame rate
      //header.meta.framerate
        hr = MFSetAttributeRatio(
            pType,
            MF_MT_FRAME_RATE, 
            (uint32_t)header.framerate ,
            1
            );
    }

    if (SUCCEEDED(hr))
    {
        // Average bit rate

        hr = pType->SetUINT32(MF_MT_AVG_BITRATE, header.videodatarate);
    }
    if (SUCCEEDED(hr))
    {
        // Sequence header.

    }

    if (SUCCEEDED(hr))
    {
        *ppType = pType;
        (*ppType)->AddRef();
    }

    SafeRelease(&pType);
    return hr;
}


HRESULT CreateAudioMediaType(const flv_file_header& header, IMFMediaType **ppType)
{
    HRESULT hr = S_OK;
    IMFMediaType *pType = NULL;

    WAVEFORMATEX format;
    ZeroMemory(&format, sizeof(format));

    format.wFormatTag = WAVE_FORMAT_RAW_AAC1;  // raw aac1
    format.nChannels = header.stereo + 1;
    format.nSamplesPerSec = header.audiosamplerate;
    format.wBitsPerSample = header.audiosamplesize;
    format.nAvgBytesPerSec = header.audiodatarate / 8;
    format.nBlockAlign = 8; // why 8
    format.cbSize = 0;

    // Use the structure to initialize the Media Foundation media type.
    hr = MFCreateMediaType(&pType);
    if (SUCCEEDED(hr))
    {
        hr = MFInitMediaTypeFromWaveFormatEx(pType, &format, sizeof(format));
    }

    if (SUCCEEDED(hr))
    {
        *ppType = pType;
        (*ppType)->AddRef();
    }

    SafeRelease(&pType);
    return hr;
}

// Get the major media type from a stream descriptor.
HRESULT GetStreamMajorType(IMFStreamDescriptor *pSD, GUID *pguidMajorType)
{
    if (pSD == NULL) { return E_POINTER; }
    if (pguidMajorType == NULL) { return E_POINTER; }

    HRESULT hr = S_OK;
    IMFMediaTypeHandler *pHandler = NULL;

    hr = pSD->GetMediaTypeHandler(&pHandler);
    if (SUCCEEDED(hr))
    {
        hr = pHandler->GetMajorType(pguidMajorType);
    }
    SafeRelease(&pHandler);
    return hr;
}


BOOL SampleRequestMatch(SourceOp *pOp1, SourceOp *pOp2)
{
    if ((pOp1 == NULL) && (pOp2 == NULL))
    {
        return TRUE;
    }
    else if ((pOp1 == NULL) || (pOp2 == NULL))
    {
        return FALSE;
    }
    else
    {
        return (pOp1->Data().ulVal == pOp2->Data().ulVal);
    }
}


#pragma warning( pop )