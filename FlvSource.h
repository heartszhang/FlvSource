#pragma once

//#include <new.h>
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
#include <vector>

#pragma comment(lib, "mfplat")
#pragma comment(lib, "mfuuid")      // Media Foundation GUIDs
#pragma comment(lib, "strmiids")    // DirectShow GUIDs
#pragma comment(lib, "Ws2_32")      // htonl, etc
#pragma comment(lib, "shlwapi")
#pragma comment(lib,"wmcodecdspuuid")
// Common sample files.
#include "InterfaceList.hpp"

#include "asynccallback.hpp"


template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

void DllAddRef();
void DllRelease();

// Forward declares
class FlvByteStreamHandler;
class FlvSource;
class FlvStream;
class SourceOp;

typedef InterfaceList<IMFSample>       SampleList;
typedef InterfaceList<IUnknown>  TokenList;    // List of tokens for IMFMediaStream::RequestSample

enum SourceState
{
    STATE_INVALID,      // Initial state. Have not started opening the stream.
    STATE_OPENING,      // BeginOpen is in progress.
    STATE_STOPPED,
    STATE_PAUSED,
    STATE_STARTED,
    STATE_SHUTDOWN
};

#include "FlvParse.hpp"          // Flv parser
#include "FlvStream.h"    // Flv stream


// Represents a request for an asynchronous operation.
/*
class SourceOp : public IUnknown
{
public:

    enum Operation
    {
        OP_START,
        OP_PAUSE,
        OP_STOP,
        OP_REQUEST_DATA,
        OP_END_OF_STREAM
    };

    static HRESULT CreateOp(Operation op, SourceOp **ppOp);
    static HRESULT CreateStartOp(IMFPresentationDescriptor *pPD, SourceOp **ppOp);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    SourceOp(Operation op);
    virtual ~SourceOp();

    HRESULT SetData(const PROPVARIANT& var);

    Operation Op() const { return m_op; }
    const PROPVARIANT& Data() { return m_data;}

protected:
    long        m_cRef;     // Reference count.
    Operation   m_op;
    PROPVARIANT m_data;     // Data for the operation.
};

class StartOp : public SourceOp
{
public:
    StartOp(IMFPresentationDescriptor *pPD);
    ~StartOp();

    HRESULT GetPresentationDescriptor(IMFPresentationDescriptor **ppPD);

protected:
    IMFPresentationDescriptor   *m_pPD; // Presentation descriptor for Start operations.

};
*/
// FlvSource: The media source object.
class FlvSource : public IMFMediaSource
{
public:
    static HRESULT CreateInstance(FlvSource **ppSource);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

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

    // Called by the byte stream handler.
    HRESULT BeginOpen(IMFByteStream *pStream, IMFAsyncCallback *pCB, IUnknown *pUnkState);
    HRESULT EndOpen(IMFAsyncResult *pResult);

    // Queues an asynchronous operation, specify by op-type.
    // (This method is public because the streams call it.)
    //HRESULT QueueAsyncOperation(SourceOp::Operation OpType);
    // called by streams
    HRESULT AsyncRequestData();
    HRESULT AsyncEndOfStream();
    // Holds and releases the source's critical section. Called by the streams.
    void    Lock() { EnterCriticalSection(&m_critSec); }
    void    Unlock() { LeaveCriticalSection(&m_critSec); }
private:

    FlvSource(HRESULT& hr);
    ~FlvSource();

    // CheckShutdown: Returns MF_E_SHUTDOWN if the source was shut down.
    HRESULT CheckShutdown() const
    {
        return ( m_state == STATE_SHUTDOWN ? MF_E_SHUTDOWN : S_OK );
    }
    // Invoke the async callback to complete the BeginOpen operation.
    HRESULT     CompleteOpen(HRESULT hrStatus);

    HRESULT     IsInitialized() const;
    /*
    BOOL        IsStreamTypeSupported(StreamType type) const;
    BOOL        IsStreamActive(const FlvPacketHeader& packetHdr);
    */

    HRESULT AsyncStart(IMFPresentationDescriptor*, PROPVARIANT const*);
    HRESULT AsyncStop();
    HRESULT AsyncPause();
    HRESULT AsyncDo(IMFAsyncCallback*, IUnknown*);

//    friend MFAsyncCallback;
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
    void        StreamingError(HRESULT hr);

    HRESULT     BeginAsyncOp();
    HRESULT     CompleteAsyncOp();

    HRESULT     DoOperation(SourceOp *pOp);
    HRESULT     ValidateOperation();

private:
    long                        m_cRef;                     // reference count

    CRITICAL_SECTION            m_critSec;                  // critical section for thread safety
    SourceState                 m_state;                    // Current state (running, stopped, paused)
    struct {
      uint32_t pending_request                        : 1;
      uint32_t aac_audio_spec_config_ready            : 1;
      uint32_t avc_decoder_configuration_record_ready : 1;
      uint32_t first_video_tag_ready                  : 1;
      uint32_t first_audio_tag_ready                  : 1;
      uint32_t on_meta_data_ready                     : 1;
      uint32_t processing_op                          : 1;
      uint32_t code_private_data_sent                 : 1;
    }status;

    flv_parser                      parser;
    flv_file_header                 header;
    IMFMediaEventQueuePtr           m_pEventQueue;             // Event generator helper
    IMFPresentationDescriptorPtr    presentation_descriptor; // Presentation descriptor.
    IMFAsyncResult                  *m_pBeginOpenResult;        // Result object for async BeginOpen operation.
    IMFByteStreamPtr                byte_stream;
    IMFMediaStreamPtr               video_stream;
    IMFMediaStreamPtr               audio_stream;

    DWORD                       m_cPendingEOS;              // Pending EOS notifications.
    ULONG                       m_cRestartCounter;          // Counter for sample requests.


    // Async callback helper.
    AsyncCallback<FlvSource> on_flv_header;
    AsyncCallback<FlvSource> on_tag_header;
//    AsyncCallback<FlvSource> on_seek_to_next_tag;
    AsyncCallback<FlvSource> on_meta_data;
    AsyncCallback<FlvSource> on_demux_sample_header;
//    AsyncCallback<FlvSource> on_demux_sample_body;
    AsyncCallback<FlvSource> on_audio_header;
    AsyncCallback<FlvSource> on_aac_packet_type;
    AsyncCallback<FlvSource> on_audio_data;
    AsyncCallback<FlvSource> on_avc_packet_type;
    AsyncCallback<FlvSource> on_video_data;
    AsyncCallback<FlvSource> on_video_header;

    HRESULT FinishInitialize();
    HRESULT ReadFlvHeader();
    HRESULT OnFlvHeader(IMFAsyncResult *result);
    HRESULT ReadFlvTagHeader();
    HRESULT OnFlvTagHeader(IMFAsyncResult *result);
    HRESULT SeekToNextTag(::tag_header const&);
    HRESULT SeekToNextTag(uint32_t distance);
    HRESULT OnSeekToNextTag(IMFAsyncResult *result);
    HRESULT ReadMetaData(uint32_t meta_size);
    HRESULT OnMetaData(IMFAsyncResult *result);
    HRESULT ReadSampleHeader();
    HRESULT OnSampleHeader(IMFAsyncResult*result);
    HRESULT EndOfFile();
    HRESULT ReadAudioHeader(tag_header const&);
    HRESULT OnAudioHeader(IMFAsyncResult*result);

    HRESULT DeliverVideoPacket(video_packet_header const&);
    HRESULT DeliverAvcPacket(video_packet_header const&);
    HRESULT DeliverNAvcPacket(video_packet_header const&);

    HRESULT ReadVideoHeader(tag_header const&);
    HRESULT OnVideoHeader(IMFAsyncResult*result);
    HRESULT DeliverAudioPacket(audio_packet_header const&ash);
    HRESULT ReadAacPacketType(audio_packet_header const&);
    HRESULT OnAacPacketType(IMFAsyncResult*);
    HRESULT ReadAudioData(audio_packet_header const&);
    HRESULT OnAudioData(IMFAsyncResult*);
    HRESULT ReadAvcPacketType(video_packet_header const&);
    HRESULT OnAvcPacketType(IMFAsyncResult*);
    HRESULT ReadVideoData(video_packet_header const&);
    HRESULT OnVideoData(IMFAsyncResult*);
    HRESULT CheckFirstPacketsReady();

    void DemuxSample();
    bool NeedDemux();
};
