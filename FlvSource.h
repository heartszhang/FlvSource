#pragma once

#include <cassert>
#include <windows.h>
#include <mftransform.h>

#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mferror.h>
#include <uuids.h>      // MPEG-1 subtypes

#include <amvideo.h>    // VIDEOINFOHEADER definition
#include <dvdmedia.h>   // VIDEOINFOHEADER2
#include <mmreg.h>      // MPEG1WAVEFORMAT
#include <shlwapi.h>

#include <wrl\implements.h>
#include <vector>
// Common sample files.
#include "InterfaceList.hpp"

#include "asynccallback.hpp"
#include "MFMediaSourceExt.hpp"
using namespace Microsoft::WRL;

// Forward declares
class FlvByteStreamHandler;
class FlvSource;
class FlvStream;
class SourceOp;



#include "FlvParse.hpp"          // Flv parser
#include "FlvStream.h"    // Flv stream

// FlvSource: The media source object.
class FlvSource : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMFMediaSource, IMFMediaSourceExt>
{
public:
    // IMFMediaEventGenerator
    STDMETHODIMP BeginGetEvent(IMFAsyncCallback* pCallback,IUnknown* punkState);
    STDMETHODIMP EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent);
    STDMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent);
    STDMETHODIMP QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue);

    // IMFMediaSource
    STDMETHODIMP CreatePresentationDescriptor(IMFPresentationDescriptor** ppPresentationDescriptor);
    STDMETHODIMP GetCharacteristics(DWORD* pdwCharacteristics);
    STDMETHODIMP Pause();
    STDMETHODIMP Shutdown();
    STDMETHODIMP Start(
        IMFPresentationDescriptor* pPresentationDescriptor,
        const GUID* pguidTimeFormat,
        const PROPVARIANT* pvarStartPosition
    );
    STDMETHODIMP Stop();

    // IMFMediaSourceExt
    HRESULT STDMETHODCALLTYPE BeginOpen(IMFByteStream *pStream, IMFAsyncCallback *pCB, IUnknown *pUnkState);
    HRESULT STDMETHODCALLTYPE EndOpen(IMFAsyncResult *pResult);

    // Queues an asynchronous operation, specify by op-type.
    // (This method is public because the streams call it.)
    //HRESULT QueueAsyncOperation(SourceOp::Operation OpType);
    // called by streams
    HRESULT STDMETHODCALLTYPE AsyncRequestData();
    HRESULT STDMETHODCALLTYPE AsyncEndOfStream();
    // Holds and releases the source's critical section. Called by the streams.
    HRESULT  STDMETHODCALLTYPE  Lock() { EnterCriticalSection(&crit_sec); return S_OK; }
    HRESULT  STDMETHODCALLTYPE  Unlock() { LeaveCriticalSection(&crit_sec); return S_OK; }

    HRESULT RuntimeClassInitialize();
    FlvSource();
private:
    ~FlvSource();

    // CheckShutdown: Returns MF_E_SHUTDOWN if the source was shut down.
    HRESULT CheckShutdown() const
    {
      return (m_state == SourceState::STATE_SHUTDOWN ? MF_E_SHUTDOWN : S_OK);
    }
    // Invoke the async callback to complete the BeginOpen operation.
    HRESULT     CompleteOpen(HRESULT hrStatus);

    HRESULT     IsInitialized() const;

    HRESULT AsyncStart(IMFPresentationDescriptor*, PROPVARIANT const*);
    HRESULT AsyncStop();
    HRESULT AsyncPause();
    HRESULT AsyncDo(IMFAsyncCallback*, IUnknown*);

    HRESULT     DoStart(IMFPresentationDescriptor*, PROPVARIANT const*);
    HRESULT     DoStop();
    HRESULT     DoPause();

    HRESULT     DoRequestData();
    HRESULT     DoEndOfStream();

    HRESULT     InitPresentationDescriptor();
    HRESULT     SelectStreams(IMFPresentationDescriptor *pPD, const PROPVARIANT *varStart);

    HRESULT   CreateAudioStream();
    HRESULT   CreateVideoStream();
    HRESULT   CreateStream(DWORD index, IMFMediaType*media_type, IMFMediaStream**v);
    HRESULT   ValidatePresentationDescriptor(IMFPresentationDescriptor *pPD);

    // Handler for async errors.
    void      StreamingError(HRESULT hr);

    void      enter_op();
    void      leave_op();

    HRESULT   DoOperation(SourceOp *pOp);
    HRESULT   ValidateOperation();

private:
    CRITICAL_SECTION            crit_sec;                  // critical section for thread safety
    SourceState                 m_state = SourceState::STATE_INVALID;    // Current state (running, stopped, paused)
    struct {
      uint32_t pending_request                        : 1;
      uint32_t aac_audio_spec_config_ready            : 1;
      uint32_t avc_decoder_configuration_record_ready : 1;
      uint32_t first_video_tag_ready                  : 1;
      uint32_t first_audio_tag_ready                  : 1;
      uint32_t on_meta_data_ready                     : 1;
      uint32_t processing_op                          : 1;
      uint32_t code_private_data_sent                 : 1;
      uint32_t pending_seek : 1;
    }status;

    flv_parser                      parser;
    flv_file_header                 header;
    IMFMediaEventQueuePtr           event_queue;             // Event generator helper
    IMFPresentationDescriptorPtr    presentation_descriptor; // Presentation descriptor.
    IMFAsyncResultPtr               begin_open_caller_result;        // Result object for async BeginOpen operation.
    IMFByteStreamPtr                byte_stream;
    IMFMediaStreamPtr               video_stream;
    IMFMediaStreamPtr               audio_stream;

    DWORD                       pending_eos = 0;              // Pending EOS notifications.
    ULONG                       restart_counter = 0;          // Counter for sample requests.
    uint64_t                    pending_seek_file_position = 0;

    // Async callback helper.
    AsyncCallback<FlvSource> on_flv_header;
    AsyncCallback<FlvSource> on_tag_header;
    AsyncCallback<FlvSource> on_meta_data;
    AsyncCallback<FlvSource> on_demux_sample_header;
    AsyncCallback<FlvSource> on_audio_header;
    AsyncCallback<FlvSource> on_aac_packet_type;
    AsyncCallback<FlvSource> on_audio_data;
    AsyncCallback<FlvSource> on_avc_packet_type;
    AsyncCallback<FlvSource> on_video_data;
    AsyncCallback<FlvSource> on_video_header;

    HRESULT FinishInitialize();
    HRESULT ReadFlvHeader();
    HRESULT STDMETHODCALLTYPE OnFlvHeader(IMFAsyncResult *result);

    HRESULT ReadFlvTagHeader();
    HRESULT STDMETHODCALLTYPE OnFlvTagHeader(IMFAsyncResult *result);

    HRESULT SeekToNextTag(::tag_header const&);
    HRESULT SeekToNextTag(uint32_t distance);
    HRESULT STDMETHODCALLTYPE OnSeekToNextTag(IMFAsyncResult *result);

    HRESULT ReadMetaData(uint32_t meta_size);
    HRESULT STDMETHODCALLTYPE OnMetaData(IMFAsyncResult *result);

    HRESULT ReadSampleHeader();
    HRESULT STDMETHODCALLTYPE OnSampleHeader(IMFAsyncResult*result);

    HRESULT ReadAudioHeader(tag_header const&);
    HRESULT STDMETHODCALLTYPE OnAudioHeader(IMFAsyncResult*result);

    HRESULT DeliverVideoPacket(video_packet_header const&);
    HRESULT DeliverAvcPacket(video_packet_header const&);
    HRESULT DeliverNAvcPacket(video_packet_header const&);
    HRESULT DeliverAudioPacket(audio_packet_header const&ash);

    HRESULT ReadVideoHeader(tag_header const&);
    HRESULT STDMETHODCALLTYPE OnVideoHeader(IMFAsyncResult*result);

    HRESULT ReadAacPacketType(audio_packet_header const&);
    HRESULT STDMETHODCALLTYPE OnAacPacketType(IMFAsyncResult*);

    HRESULT ReadAudioData(audio_packet_header const&);
    HRESULT STDMETHODCALLTYPE OnAudioData(IMFAsyncResult*);

    HRESULT ReadAvcPacketType(video_packet_header const&);
    HRESULT STDMETHODCALLTYPE OnAvcPacketType(IMFAsyncResult*);

    HRESULT ReadVideoData(video_packet_header const&);
    HRESULT STDMETHODCALLTYPE OnVideoData(IMFAsyncResult*);


    HRESULT EndOfFile();
    HRESULT CheckFirstPacketsReady();

    void DemuxSample();
    bool NeedDemux();
};
