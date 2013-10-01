#include "FlvSource.h"
#include <wmcodecdsp.h>
#include "AsyncCallback.hpp"
#include "MFState.hpp"
#include "avcc.hpp"
#pragma warning( push )
#pragma warning( disable : 4355 )  // 'this' used in base member initializer list


HRESULT CreateVideoMediaType(const flv_file_header& , IMFMediaType **ppType);
HRESULT CreateAudioMediaType(const flv_file_header& , IMFMediaType **ppType);
HRESULT GetStreamMajorType(IMFStreamDescriptor *pSD, GUID *pguidMajorType);
//BOOL    SampleRequestMatch(SourceOp *pOp1, SourceOp *pOp2);
HRESULT NewMFMediaBuffer(const uint8_t*data, uint32_t length, IMFMediaBuffer **rtn);
HRESULT NewNaluBuffer(uint8_t nallength, packet const&nalu, IMFMediaBuffer **rtn);

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
      hr = AsyncPause();// QueueAsyncOperation(SourceOp::OP_PAUSE);
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

    hr = CheckShutdown();

    if (SUCCEEDED(hr))
    {
        // Shut down the stream objects.
      if (audio_stream){
        auto as = static_cast<FlvStream*>(audio_stream.GetInterfacePtr());
        as->Shutdown();
      }
      if (video_stream){
        auto vs = static_cast<FlvStream*>(video_stream.GetInterfacePtr());
        vs->Shutdown();
      }
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
        status.processing_op = 0;

        // Set the state.
        m_state = STATE_SHUTDOWN;
    }

    LeaveCriticalSection(&m_critSec);
    return hr;
}

struct _prop_variant_t : PROPVARIANT{
  _prop_variant_t();
  ~_prop_variant_t();
  _prop_variant_t(const _prop_variant_t&);
  _prop_variant_t(PROPVARIANT const*);
  _prop_variant_t&operator=(_prop_variant_t&);
  _prop_variant_t&operator=(PROPVARIANT const*);
};
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
    hr = AsyncStart(pPresentationDescriptor, pvarStartPos);
done:
    LeaveCriticalSection(&m_critSec);
    return hr;
}

HRESULT FlvSource::AsyncStart(IMFPresentationDescriptor* pd, PROPVARIANT const*startpos){
  IMFPresentationDescriptorPtr spd(pd);
  _prop_variant_t spos = startpos;
  return AsyncDo(MFAsyncCallback::New([spd, spos, this](IMFAsyncResult*result)->HRESULT{
    auto hr = this->DoStart(spd, &spos);
    result->SetStatus(hr);
    return S_OK;
  }), this);  // state add this's ref
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
      hr = AsyncStop();// QueueAsyncOperation(SourceOp::OP_STOP);
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
  if (tagh.type == flv::tag_type::script_data && !status.on_meta_data_ready){
    header.status.has_script_data = 1;
//    status.on_meta_data_ready = 1;
    ReadMetaData(tagh.data_size);
  }
  else if (tagh.type == flv::tag_type::video ){
    header.status.has_video = 1;
    if (!header.first_media_tag_offset)
      header.first_media_tag_offset = tagh.data_offset - flv::flv_tag_header_length;
    if(status.first_video_tag_ready)
      SeekToNextTag(tagh);
    else ReadVideoHeader(tagh);
  }
  else if (tagh.type == flv::tag_type::audio){
    header.status.has_audio = 1;
    if (!header.first_media_tag_offset)
      header.first_media_tag_offset = tagh.data_offset - flv::flv_tag_header_length;
    if (status.first_audio_tag_ready)
      SeekToNextTag(tagh);
    else ReadAudioHeader(tagh);
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
  QWORD pos = 0;
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
  status.on_meta_data_ready = 1;
  header.status.meta_ready = 1;
  if (header.audiocodecid != flv::audio_codec::aac && header.videocodecid != flv::video_codec::avc)
    FinishInitialize();
  else  ReadFlvTagHeader();
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

/* Private methods */

FlvSource::FlvSource(HRESULT& hr) :
    m_cRef(1),
    m_pEventQueue(NULL),
    presentation_descriptor(NULL),
    m_pBeginOpenResult(NULL),
    byte_stream(NULL),
    m_state(STATE_INVALID),
    m_cRestartCounter(0),
    on_flv_header(this, &FlvSource::OnFlvHeader),
    on_tag_header(this, &FlvSource::OnFlvTagHeader),
    on_meta_data(this, &FlvSource::OnMetaData),
    on_demux_sample_header(this, &FlvSource::OnSampleHeader),
    on_audio_header(this, &FlvSource::OnAudioHeader),
    on_aac_packet_type(this, &FlvSource::OnAacPacketType),
    on_audio_data(this, &FlvSource::OnAudioData),
    on_avc_packet_type(this, &FlvSource::OnAvcPacketType),
    on_video_data(this, &FlvSource::OnVideoData),
    on_video_header(this, &FlvSource::OnVideoHeader)
{
  ZeroMemory(&status, sizeof(status));
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

/*
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
*/
//-------------------------------------------------------------------
// BeginAsyncOp
//
// Starts an asynchronous operation. Called by the source at the
// begining of any asynchronous operation.
//-------------------------------------------------------------------

HRESULT FlvSource::BeginAsyncOp()
{
  status.processing_op = 1;
    return S_OK;
}

//-------------------------------------------------------------------
// CompleteAsyncOp
//
// Completes an asynchronous operation. Called by the source at the
// end of any asynchronous operation.
//-------------------------------------------------------------------

HRESULT FlvSource::CompleteAsyncOp()
{
    assert(status.processing_op);
    status.processing_op = 0;
    // Process the next operation on the queue.
//    auto hr = DispatchOperations();
    return S_OK;
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

HRESULT FlvSource::ValidateOperation()
{
  return (status.processing_op) ? MF_E_NOTACCEPTING : S_OK;
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

HRESULT FlvSource::DoStart(IMFPresentationDescriptor*pd, PROPVARIANT const*startpos)
{
  auto hr = ValidateOperation();
  assert(ok(hr));  // overlapped operations arenot permitted
  hr = BeginAsyncOp();

    // Because this sample does not support seeking, the start
    // position must be 0 (from stopped) or "current position."

    // If the sample supported seeking, we would need to get the
    // start position from the PROPVARIANT data contained in pOp.

    // Select/deselect streams, based on what the caller set in the PD.
    // This method also sends the MENewStream/MEUpdatedStream events.
    hr = SelectStreams(pd, startpos);

    if (SUCCEEDED(hr))
    {
        m_state = STATE_STARTED;

        // Queue the "started" event. The event data is the start position.
        hr = m_pEventQueue->QueueEventParamVar(
            MESourceStarted,
            GUID_NULL,
            S_OK,
            startpos
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

    CompleteAsyncOp();

    return hr;
}

//-------------------------------------------------------------------
// SelectStreams
// Called during START operations to select and deselect streams.
// This method also sends the MENewStream/MEUpdatedStream events.
//-------------------------------------------------------------------
HRESULT     FlvSource::SelectStreams(IMFPresentationDescriptor *pPD, const PROPVARIANT *varStart){
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
    else if (stream_id == 0){
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

HRESULT FlvSource::AsyncStop(){
  return AsyncDo(MFAsyncCallback::New([this](IMFAsyncResult*result)->HRESULT{
    auto hr = this->DoStop();
    result->SetStatus(hr);
    return S_OK;
  }), this);
}
//-------------------------------------------------------------------
// DoStop
// Perform an async stop operation (IMFMediaSource::Stop)
//-------------------------------------------------------------------

HRESULT FlvSource::DoStop()
{
    HRESULT hr = S_OK;
    QWORD qwCurrentPosition = 0;

    hr = BeginAsyncOp();

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

    CompleteAsyncOp();

    return hr;
}

HRESULT FlvSource::AsyncPause(){
  return AsyncDo(MFAsyncCallback::New([this](IMFAsyncResult*result)->HRESULT{
    auto hr = this->DoPause();
    result->SetStatus(hr);
    return S_OK;
  }), this);
}

//-------------------------------------------------------------------
// DoPause
// Perform an async pause operation (IMFMediaSource::Pause)
//-------------------------------------------------------------------

HRESULT FlvSource::DoPause()
{
    HRESULT hr = S_OK;

    hr = BeginAsyncOp();

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

    CompleteAsyncOp();

    return hr;
}

HRESULT FlvSource::AsyncRequestData(){
  return AsyncDo(MFAsyncCallback::New([this](IMFAsyncResult*result)->HRESULT{
    auto hr = this->DoRequestData();
    result->SetStatus(hr);
    return S_OK;
  }), this);
}
//-------------------------------------------------------------------
// StreamRequestSample
// Called by streams when they need more data.
//
// Note: This is an async operation. The stream requests more data
// by queueing an OP_REQUEST_DATA operation.
//-------------------------------------------------------------------

HRESULT FlvSource::DoRequestData()
{
    BeginAsyncOp();
    DemuxSample();
    HRESULT hr = CompleteAsyncOp();
    return hr;
}

void FlvSource::DemuxSample(){
  if (!NeedDemux())
    return;
  status.pending_request = 1;
  ReadSampleHeader();
}
HRESULT FlvSource::ReadSampleHeader(){
  auto hr= parser.begin_tag_header(1, &on_demux_sample_header, nullptr);
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
  video_packet_header vsh(FromAsyncResultState<tag_header>(result), vh);
  if (ok(hr)){
    if (vsh.codec_id == flv::video_codec::avc) {
      ReadAvcPacketType(vsh);
    }
    else{
//      byte_stream->GetCurrentPosition(&vsh.payload_offset);
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
  audio_packet_header ash(th, ah);
  if (ok(hr)){
    if (ah.codec_id == flv::audio_codec::aac)
      ReadAacPacketType(ash); // ignore return value
    else {
//      byte_stream->GetCurrentPosition(&ash.payload_offset);
      ReadAudioData(ash);
    }
  }
  if (fail(hr)){
    Shutdown();
  }
  return hr;
}

HRESULT FlvSource::ReadAacPacketType(audio_packet_header const&ash){
  auto hr = parser.begin_aac_packet_type(&on_aac_packet_type, NewMFState(ash));
  if (fail(hr))
    Shutdown();
  return hr;
}
HRESULT FlvSource::OnAacPacketType(IMFAsyncResult*result){
  auto &ash = FromAsyncResultState<audio_packet_header>(result);
  auto hr = parser.end_aac_packet_type(result, &ash.aac_packet_type);
//  if (ok(hr)) 
//    hr = byte_stream->GetCurrentPosition(&ash.payload_offset);
  if (ok(hr))
    ReadAudioData(ash);

  if (fail(hr))
    Shutdown();
  return hr;
}

HRESULT FlvSource::ReadAvcPacketType(video_packet_header const&vsh){
  auto hr = parser.begin_avc_header(&on_avc_packet_type, NewMFState(vsh));
  if (fail(hr))
    Shutdown();
  return hr;
}
HRESULT FlvSource::OnAvcPacketType(IMFAsyncResult*result){
  avc_header v;
  auto hr = parser.end_avc_header(result, &v);
  auto &vsh = FromAsyncResultState<video_packet_header>(result);
  vsh.avc_packet_type = v.avc_packet_type;
  vsh.composition_time = v.composite_time;
//  byte_stream->GetCurrentPosition(&vsh.payload_offset);
  if (ok(hr))
   //  OnVideoHeaderReady(vsh);
    ReadVideoData(vsh);
    
  if (fail(hr))
    Shutdown();
  return hr;
}
HRESULT FlvSource::ReadAudioData(audio_packet_header const& ash){
  auto hr = parser.begin_audio_data(ash.payload_length(), &on_audio_data, NewMFState<audio_packet_header>(ash));
  if (fail(hr))
    Shutdown();
  return hr;
}
HRESULT FlvSource::DeliverAudioPacket(audio_packet_header const&ash){
  IMFMediaBufferPtr mbuf;
  auto hr = NewMFMediaBuffer(ash.payload._, ash.payload.length, &mbuf);

  IMFSamplePtr sample;
  if (ok(hr))
    hr = MFCreateSample(&sample);
  if(ok(hr)) hr = sample->AddBuffer(mbuf);
  if (ok(hr)) hr = sample->SetSampleTime(ash.nano_timestamp);
  if (ok(hr)){
    auto astream = static_cast<FlvStream*>(audio_stream.GetInterfacePtr());
    hr = astream->DeliverPayload(sample);
  }
  status.pending_request = 0;
  DemuxSample();
  return hr;
}
HRESULT FlvSource::OnAudioData(IMFAsyncResult *result){
  auto &ash = FromAsyncResultState<audio_packet_header>(result);
//  packet pack;
  auto  hr = parser.end_audio_data(result, &ash.payload);
  if (ok(hr) && status.first_audio_tag_ready){
    hr = DeliverAudioPacket(ash);
  }
  else if(ok(hr)){
    status.first_audio_tag_ready = 1;
    header.audio = ash;
    CheckFirstPacketsReady();
  }
  if (fail(hr))
    Shutdown();
  return hr;
}
HRESULT FlvSource::CheckFirstPacketsReady(){
  auto ar = !header.has_audio || header.audiocodecid != flv::audio_codec::aac || status.first_audio_tag_ready;
  auto vr = !header.has_video || header.videocodecid != flv::video_codec::avc || status.first_video_tag_ready;
  HRESULT hr = S_OK;
  if (ar && vr){
    hr = FinishInitialize();
  }
  else {
    hr = ReadFlvTagHeader();
  }
  return hr;
}
HRESULT FlvSource::ReadVideoData(video_packet_header const& vsh){
  auto hr = parser.begin_video_data(vsh.payload_length(), &on_video_data, NewMFState<video_packet_header>(vsh));
  if (fail(hr))
    Shutdown();
  return hr;
}
HRESULT FlvSource::DeliverAvcPacket(video_packet_header const&vsh){
  IMFSamplePtr sample;
  auto  hr = MFCreateSample(&sample);
  if (!status.code_private_data_sent){
    status.code_private_data_sent = 1;
    IMFMediaBufferPtr cpdbuf;
    auto cpd = header.avcc.code_private_data();
    hr = NewMFMediaBuffer(cpd._, cpd.length, &cpdbuf);
    if (ok(hr))
      hr = sample->AddBuffer(cpdbuf);
  }

  if (vsh.avc_packet_type == flv::avc_packet_type::avc_nalu){
    auto nal = header.avcc.nal;
    flv::nalu_reader reader(vsh.payload._, vsh.payload.length);
    for (auto nalu = reader.nalu(); nalu.length; nalu = reader.nalu()){
      IMFMediaBufferPtr mbuf;
      hr = NewNaluBuffer(nal, nalu, &mbuf);//MFCreateMemoryBuffer(vsh.payload.length, &mbuf);
      if (ok(hr)) hr = sample->AddBuffer(mbuf);
    }
    assert(reader.pointer == reader.length);
  }

  if (ok(hr)) hr = sample->SetSampleTime(vsh.nano_timestamp);
  if (ok(hr)) hr = sample->SetUINT32(MFSampleExtension_CleanPoint, vsh.frame_type == flv::frame_type::key_frame ? 1 : 0);
  // should set sample duration

  if (ok(hr)){
    auto astream = static_cast<FlvStream*>(video_stream.GetInterfacePtr());
    hr = astream->DeliverPayload(sample);
  }
  status.pending_request = 0;
  DemuxSample();
  return hr;
}
HRESULT FlvSource::DeliverNAvcPacket(video_packet_header const&vsh){
  IMFSamplePtr sample;
  auto  hr = MFCreateSample(&sample);
  IMFMediaBufferPtr mbuf;
  hr = NewMFMediaBuffer(vsh.payload._, vsh.payload.length, &mbuf);
  if (ok(hr)) hr = sample->AddBuffer(mbuf);

  if (ok(hr)) hr = sample->SetSampleTime(vsh.nano_timestamp);
  if (ok(hr)) hr = sample->SetUINT32(MFSampleExtension_CleanPoint, vsh.frame_type == flv::frame_type::key_frame ? 1 : 0);
  // should set sample duration

  if (ok(hr)){
    auto astream = static_cast<FlvStream*>(video_stream.GetInterfacePtr());
    hr = astream->DeliverPayload(sample);
  }
  status.pending_request = 0;
  DemuxSample();
  return hr;
}
HRESULT FlvSource::DeliverVideoPacket(video_packet_header const& vsh){
  if (vsh.codec_id == flv::video_codec::avc)
    return DeliverAvcPacket(vsh);
  else
    return DeliverNAvcPacket(vsh);

}
HRESULT FlvSource::OnVideoData(IMFAsyncResult *result){
  auto &ash = FromAsyncResultState<video_packet_header>(result);
  auto  hr = parser.end_video_data(result, &ash.payload);
  if (ok(hr) && status.first_video_tag_ready){
    hr = DeliverVideoPacket(ash);
  }
  else if (ok(hr)){
    status.first_video_tag_ready = 1;
    header.video = ash;
    header.avcc = flv::avcc_reader(ash.payload._, ash.payload.length).avcc();
    CheckFirstPacketsReady();
  }

  if (fail(hr))
    Shutdown();
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

HRESULT FlvSource::AsyncEndOfStream(){
  return AsyncDo(MFAsyncCallback::New([this](IMFAsyncResult*result)->HRESULT{
    auto hr = this->DoEndOfStream();
    result->SetStatus(hr);
    return S_OK;
  }), this);
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

HRESULT FlvSource::DoEndOfStream()
{
    HRESULT hr = S_OK;

    hr = BeginAsyncOp();

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
        hr = CompleteAsyncOp();
    }

    return hr;
}

HRESULT FlvSource::FinishInitialize(){
  HRESULT hr = S_OK;
//  if (header.first_media_tag_offset) {
//    hr = byte_stream->SetCurrentPosition(header.first_media_tag_offset - flv::flv_previous_tag_size_field_length);
  //}
  CreateAudioStream();
  CreateVideoStream();
  hr = InitPresentationDescriptor();

  return hr;
}

HRESULT FlvSource::CreateStream(DWORD index, IMFMediaType*media_type, IMFMediaStream**v) {
  _com_ptr_t < _com_IIID < IMFStreamDescriptor, &__uuidof(IMFStreamDescriptor)>> pSD;
  _com_ptr_t < _com_IIID < IMFMediaTypeHandler, &__uuidof(IMFMediaTypeHandler)>> pHandler;

  auto  hr = MFCreateStreamDescriptor(index, 1, &media_type, &pSD);

  // Set the default media type on the stream handler.
  if (SUCCEEDED(hr))
  {
    hr = pSD->GetMediaTypeHandler(&pHandler);
  }
  if (SUCCEEDED(hr))
  {
    hr = pHandler->SetCurrentMediaType(media_type);
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
    hr = CreateStream(1, media_type, &audio_stream);
  return hr;
}

HRESULT FlvSource::CreateVideoStream(){
  IMFMediaTypePtr media_type;
  auto hr = CreateVideoMediaType(header, &media_type);
  if (ok(hr))
    hr = CreateStream(0, media_type, &video_stream);
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
  pPD;
    HRESULT hr = S_OK;
//    DWORD cStreams = 0;

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


/*  Static functions */


//-------------------------------------------------------------------
// CreateVideoMediaType:
// Create a media type from an MPEG-1 video sequence header.
//-------------------------------------------------------------------

HRESULT CreateVideoMediaType(const flv_file_header& header, IMFMediaType **ppType)
{
  if (header.videocodecid != flv::video_codec::avc){
    *ppType = nullptr;
    return MF_E_UNSUPPORTED_FORMAT;
  }
    HRESULT hr = S_OK;
    IMFMediaType *pType = NULL;
    hr = MFCreateMediaType(&pType);

    if (ok(hr)) hr = pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (ok(hr)) hr = pType->SetGUID(MF_MT_SUBTYPE, MEDIASUBTYPE_H264);
//    if (ok(hr)) hr = pType->SetGUID(MF_MT_SUBTYPE, MEDIASUBTYPE_AVC1);  NOT SUPPORTED
        
    if (ok(hr)) hr = MFSetAttributeSize(pType, MF_MT_FRAME_SIZE,  header.width, header.height   );
    if (ok(hr)) hr = MFSetAttributeRatio(pType,MF_MT_FRAME_RATE, header.framerate ,1);
    if (ok(hr)) hr = MFSetAttributeRatio(pType, MF_MT_FRAME_RATE_RANGE_MAX, header.framerate, 1);
    if (ok(hr)) hr = MFSetAttributeRatio(pType, MF_MT_FRAME_RATE_RANGE_MIN, header.framerate / 2, 1);
    if (ok(hr)) hr = MFSetAttributeRatio(pType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (ok(hr)) hr = pType->SetUINT32(MF_MT_AVG_BITRATE, header.videodatarate);

    // header.video.payload
    auto cpd = header.avcc.sequence_header();
    
    if (ok(hr)) hr = pType->SetUINT32(MF_MT_MPEG2_FLAGS, header.avcc.nal);  // from avcC
    if (ok(hr)) hr = pType->SetUINT32(MF_MT_MPEG2_PROFILE, header.avcc.profile); // eAVEncH264VProfile, from avcC
    if (ok(hr)) hr = pType->SetUINT32(MF_MT_MPEG2_LEVEL, header.avcc.level);    
//    if (ok(hr)) hr = pType->SetBlob(MF_MT_USER_DATA, cpd._, cpd.length); // with no effect
    if (ok(hr)) hr = pType->SetBlob(MF_MT_MPEG_SEQUENCE_HEADER, cpd._, cpd.length);
// CodecPrivateData issue of Smooth Streaming 
// H264: exactly same as stated in the spec.The field is in NAL byte stream : 0x00 0x00 0x00 0x01 SPS 0x00 0x00 0x00 0x01 PPS.No problem

//    if (ok(hr)) hr = pType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (ok(hr))
    {
        *ppType = pType;
        (*ppType)->AddRef();
    }

    SafeRelease(&pType);
    return hr;
}

HRESULT CreateAudioMediaType(const flv_file_header& header, IMFMediaType **ppType)
{    
  auto cid = header.audiocodecid;
  if (cid != flv::audio_codec::aac && cid != flv::audio_codec::mp3 && cid != flv::audio_codec::mp38k){
    *ppType = nullptr;
    return MF_E_UNSUPPORTED_FORMAT; 
  }
  // MEDIASUBTYPE_MPEG_HEAAC not work
    IMFMediaType *pType = NULL;
    auto hr = MFCreateMediaType(&pType);
    if (ok(hr)) hr = pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (ok(hr) && header.audiocodecid == flv::audio_codec::aac) hr = pType->SetGUID(MF_MT_SUBTYPE, MEDIASUBTYPE_RAW_AAC1);
    if (ok(hr) && header.audiocodecid == flv::audio_codec::mp3 || header.audiocodecid == flv::audio_codec::mp38k)
      hr = pType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_MP3);
    if (ok(hr) && header.audiosamplerate) hr = pType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, header.audiosamplerate);
    if (ok(hr)) hr = pType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, header.stereo + 1);
    if (ok(hr)) hr = pType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1);
    if (ok(hr)) hr = pType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, header.audiosamplesize);
    if (ok(hr)) hr = pType->SetBlob(MF_MT_USER_DATA, header.audio.payload._, header.audio.payload.length);

    if (ok(hr))
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


HRESULT FlvSource::AsyncDo(IMFAsyncCallback* invoke, IUnknown* state){
  auto hr = MFPutWorkItem( MFASYNC_CALLBACK_QUEUE_STANDARD,  invoke,  state);
  return hr;
}

HRESULT NewMFMediaBuffer(const uint8_t*data, uint32_t length, IMFMediaBuffer **rtn){
  IMFMediaBufferPtr v;
  auto hr = MFCreateMemoryBuffer(length, &v);
  uint8_t* buffer = nullptr;
  uint32_t max_length = 0;
  if (ok(hr))
    hr = v->Lock(&buffer, (DWORD*)&max_length, nullptr);
  if (buffer){
    memcpy_s(buffer, max_length, data, length);
    v->SetCurrentLength(length);
  }
  if (ok(hr))
    hr = v->Unlock();
  if (ok(hr)){
    *rtn = v;
    (*rtn)->AddRef();
  }
  return hr;
}
HRESULT NewNaluBuffer(uint8_t nallength, packet const&nalu, IMFMediaBuffer **rtn){
  IMFMediaBufferPtr v;
  auto hr = MFCreateMemoryBuffer(nallength + nalu.length, &v);
  uint8_t* buffer = nullptr;
  uint32_t max_length = 0;
  if (ok(hr))
    hr = v->Lock(&buffer, (DWORD*)&max_length, nullptr);
  uint32_t startcode = 0x00000001;

  if (buffer){
    bigendian::binary_writer writer(buffer, max_length);
    if (nallength < 4)
      writer.ui24(startcode);
    else writer.ui32(startcode);
    writer.packet(nalu);
    v->SetCurrentLength(max_length);
  }
  if (ok(hr))
    hr = v->Unlock();
  if (ok(hr)){
    *rtn = v;
    (*rtn)->AddRef();
  }
  return hr;
}
#pragma warning( pop )

_prop_variant_t::~_prop_variant_t(){
  PropVariantClear(this);
}
_prop_variant_t::_prop_variant_t(PROPVARIANT const*p){
  PropVariantCopy(this, p);
}
_prop_variant_t::_prop_variant_t(_prop_variant_t const&rhs){
  PropVariantCopy(this, &rhs);
}
