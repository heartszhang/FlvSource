#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <algorithm>
#include <comdef.h>
#include <comip.h>
#include <mfidl.h>
#include <mfapi.h>
#include "bigendian.hpp"
#include "flv_meta.hpp"
#include "buffer.hpp"
#include "flv.hpp"


struct aac_audio_spec_config;// iso-14496-3
struct aac_raw_frame_data;
struct flv_header{
  uint8_t version;
  uint8_t has_video;
  uint8_t has_audio;
};
struct tag_header {
  flv::tag_type type;
  int8_t        filter;       //1 encrypted, 0 : no pre-preocessing
  uint32_t      data_size;    // message size bytes
  uint32_t      timestamp;    //milliseconds
  uint64_t      data_offset;  // fileposition of payload
  uint64_t      nano_time()const{
    return 10000 * uint64_t(timestamp); // to nano seconds
  }
  tag_header(tag_header const&) = default;
  tag_header() = default;
};


struct audio_header {
  flv::audio_codec         codec_id;
  flv::sound_rate          sound_rate;  // kbits
  flv::sound_size          sound_size; // 8 /16 bits
  flv::sound_type          sound_type;// 1 : stereo, 0 : mono
};

struct video_header {
  flv::frame_type  frame_type;
  flv::video_codec codec_id;
};
struct avc_header{
  flv::avc_packet_type avc_packet_type;
  uint32_t composite_time;
};
// be filled after parse audio-tag header
struct audio_stream_header : public tag_header{
  int32_t                       stream_id;  // Raw stream_id field.  // must be 0
  flv::audio_codec              codec_id;   // 10 = AAC
  flv::sound_rate               sound_rate; // 0: 5.5k, 1: 11k, 2: 22k, 3:44k
  flv::sound_size               sound_size; // 0 : 8bit, 1 : 16bit
  flv::sound_type               sound_type; // 0: Mono, 1: stereo

  flv::aac_packet_type          aac_packet_type;// if codec = 10
//  aac_audio_spec_config*        audio_specific_config;
  uint64_t                      payload_offset;// file position absolute
  audio_stream_header() = default;
  explicit audio_stream_header(tag_header const&t) : tag_header(t){};
  explicit audio_stream_header(tag_header const&t, audio_header const&ah) : tag_header(t){}

  //载荷数据长度，不包含tag_header, audio_header, aac_packet_type
  uint32_t payload_length()const {
    auto v = data_size - flv::flv_tag_header_length - flv::flv_audio_header_length;
    if (codec_id == flv::audio_codec::aac)
      v -= flv::flv_aac_packet_type_length;
    return v;
  }
  uint8_t channels()const{
    return uint8_t(sound_type) + 1;
  }
  uint32_t sample_per_sec()const{
    return 44100 * (1 << uint32_t(sound_rate)) / 8;
  }
  uint32_t bits_per_sample()const{
    return 8 * (uint32_t(sound_size) + 1);
  }
};


struct avc_decoder_configuration_record;
struct avc_nalus;

struct video_stream_header : public tag_header{
  int32_t                          stream_id;  // Raw stream_id field. must be 0
  flv::frame_type                  frame_type;
  flv::video_codec                 codec_id;  // 7 = avc
 
  flv::avc_packet_type             avc_packet_type;// if codec = 7
  uint32_t                         composition_time;// if codec = 7

  uint64_t                         payload_offset;  // after composite-time
  //载荷数据长度，不包含tag_header, audio_header, avc_packet_type
  uint32_t payload_length()const{
    auto v = data_size - flv::flv_tag_header_length - flv::flv_video_header_length;
    if (codec_id == flv::video_codec::avc)
      v -= flv::flv_avc_packet_type_length; // sizeof(composite) + sizeof(avcpackettype)
    return v;
  }
  video_stream_header() = default;
  explicit video_stream_header(tag_header const&t) : tag_header(t){};
  video_stream_header(tag_header const&t, video_header const&v):tag_header(t){}
};

struct packet{
  uint8_t *_;
  uint32_t length;
  packet() :_(nullptr), length(0){}
  packet(packet const&rhs):packet(){
    if (rhs.length){
      length = rhs.length;
      _ = new uint8_t[length];
      memcpy(_, rhs._, length);
    }
  }
  packet(packet &&rhs):packet(){
    std::swap(length, rhs.length);
    std::swap(_, rhs._);
  }
  packet&operator=(packet&&rhs){
    std::swap(length, rhs.length);
    std::swap(_, rhs._);
    return *this;
  }
  packet&operator=(packet const&rhs){
    if (rhs.length){
      length = rhs.length;
      _ = new uint8_t[length];
      memcpy(_, rhs._, length);
    }
    return *this;
  }
  packet(const uint8_t*d, uint32_t len) : length(len){
    if (length){
      _ = new uint8_t[length];
      memcpy(_, d, length);
    }
  }
  ~packet(){
    if (_)
      delete[] _;
  }
};
// be filled after read the first script tag
// FlvStreamHeader
// Holds information from the system header.
struct flv_file_header : public flv_meta{
  uint64_t first_media_tag_offset;
  std::unique_ptr<video_stream_header> video;
  std::unique_ptr<audio_stream_header> audio;
  struct {
    uint32_t file_header_ready : 1;
    uint32_t meta_ready : 1;
    uint32_t has_script_data : 1;
    uint32_t has_video : 1;
    uint32_t has_audio : 1;
    uint32_t scan_once : 1;
  }status;
};

typedef _com_ptr_t < _com_IIID<IMFMediaEventQueue, &__uuidof(IMFMediaEventQueue)>>                IMFMediaEventQueuePtr;
typedef _com_ptr_t < _com_IIID < IMFPresentationDescriptor, &_uuidof(IMFPresentationDescriptor)>> IMFPresentationDescriptorPtr;
typedef _com_ptr_t < _com_IIID < IMFByteStream, &_uuidof(IMFByteStream)>>                         IMFByteStreamPtr;
typedef _com_ptr_t < _com_IIID < IMFMediaStream, &_uuidof(IMFMediaStream)>>                       IMFMediaStreamPtr;
typedef _com_ptr_t < _com_IIID<IMFMediaType, &__uuidof(IMFMediaType) >>                           IMFMediaTypePtr;
typedef _com_ptr_t < _com_IIID<IMFStreamDescriptor, &__uuidof(IMFStreamDescriptor) >>             IMFStreamDescriptorPtr;
typedef _com_ptr_t < _com_IIID<IMFSample, &__uuidof(IMFSample) >>                                 IMFSamplePtr;
typedef _com_ptr_t < _com_IIID<IUnknown, &__uuidof(IUnknown) >>                                   IUnknownPtr;
typedef _com_ptr_t < _com_IIID<IMFMediaBuffer, &__uuidof(IMFMediaBuffer) >>                       IMFMediaBufferPtr;
typedef _com_ptr_t < _com_IIID<IMFAsyncResult, &__uuidof(IMFAsyncResult)>> IMFAsyncResultPtr;

struct flv_parser : public buffer{
  IMFByteStreamPtr        stream;
  int8_t                           signature_passed;
  uint32_t                         skip_previsou_tag_size();

  // ok, fail/ or more data; return 0: passed, -1:error
  int32_t                          check_flv_header(uint8_t* has_video, uint8_t*has_audio);

  // parse flv tag header
  HRESULT                     tag_header(::tag_header*);
  HRESULT flv_header(::flv_header*);
  HRESULT                     audio_header(audio_header*rlt);
  HRESULT                     aac_packet_type(flv::aac_packet_type*rlt);
  HRESULT                     video_header(::video_header*rlt);
  HRESULT                     avc_header(::avc_header*);
  HRESULT                     on_meta_data(flv_meta*rlt);
  HRESULT audio_data(packet*);
  HRESULT video_data(packet*);

  HRESULT begin_flv_header(IMFByteStreamPtr stream, IMFAsyncCallback*, IUnknown*state);
  HRESULT end_flv_header(IMFAsyncResult*, ::flv_header*);
//  AsyncCallback<flv_parser> on_check_flv_header;

  HRESULT begin_tag_header(int8_t withprevfield, IMFAsyncCallback*, IUnknown*state);
  HRESULT end_tag_header(IMFAsyncResult*, ::tag_header*);
//  AsyncCallback<flv_parser> on_tag_header;

  HRESULT begin_audio_header(IMFAsyncCallback*, IUnknown*);
  HRESULT end_audio_header(IMFAsyncResult*, ::audio_header*);
//  AsyncCallback<flv_parser> on_audio_header;

  HRESULT begin_on_meta_data(uint32_t meta_size, IMFAsyncCallback*, IUnknown*);
  HRESULT end_on_meta_data(IMFAsyncResult*, flv_meta*);
//  AsyncCallback<flv_parser> on_on_meta_data;

  HRESULT begin_aac_packet_type(IMFAsyncCallback*, IUnknown*);
  HRESULT end_aac_packet_type(IMFAsyncResult*, flv::aac_packet_type *packet_type);
//  AsyncCallback<flv_parser> on_aac_packet_type;

  HRESULT begin_avc_header(IMFAsyncCallback*, IUnknown*);
  HRESULT end_avc_header(IMFAsyncResult*, ::avc_header*);
//  AsyncCallback<flv_parser> on_avc_packet_type;

  HRESULT begin_audio_data(uint32_t payload_length, IMFAsyncCallback*, IUnknown*);
  HRESULT end_audio_data(IMFAsyncResult*, packet*);
//  AsyncCallback<flv_parser> on_audio_data;

  HRESULT begin_video_header(IMFAsyncCallback*,IUnknown*);
  HRESULT end_video_header(IMFAsyncResult*, ::video_header*);
//  AsyncCallback<flv_parser> on_video_header;

  HRESULT begin_video_data(uint32_t payload_length, IMFAsyncCallback*, IUnknown*);
  HRESULT end_video_data(IMFAsyncResult*, packet*);
//  AsyncCallback<flv_parser> on_video_header;

  /*
  HRESULT begin_previous_tag_size(IMFAsyncCallback*, IUnknown*);
  HRESULT end_previsou_tag_size(IMFAsyncResult*, uint32_t*prev_tag_size);
  AsyncCallback<flv_parser> on_previous_tag_size;
  */
protected:
  template<typename data_t> HRESULT end_read(IMFAsyncResult*result, data_t*v);
  template<typename data_t> HRESULT begin_read(IMFAsyncCallback*cb, IUnknown*s, uint32_t length, HRESULT(flv_parser::*decoder)(data_t*));
};

template<typename data_t>
HRESULT flv_parser::begin_read(IMFAsyncCallback*cb, IUnknown*s, uint32_t length, HRESULT(flv_parser::*decoder)(data_t*)){
  Reset(length);
  IMFAsyncResultPtr caller_result;
  auto hr = MFCreateAsyncResult(new MFState<data_t>(), cb, s, &caller_result);
  hr = stream->BeginRead(
    DataPtr(),
    length,
    MFAsyncCallback::New([this, caller_result, decoder](IMFAsyncResult*result)->HRESULT{
      DWORD cb = 0;
      auto hr = this->stream->EndRead(result, &cb);
      this->MoveEnd(cb);
      if (ok(hr))
        hr = result->GetStatus();
      auto &v = FromAsyncResult<data_t>(caller_result);
      if (ok(hr))
        hr = (this->*decoder)(&v);
      caller_result->SetStatus(hr);
      MFInvokeCallback(caller_result);
      return S_OK;
  }), nullptr);
  return hr;
}
template<typename data_t>
HRESULT flv_parser::end_read(IMFAsyncResult*result, data_t*v){
  auto const&ah = FromAsyncResult<data_t>(result);
  *v = ah;
  return result->GetStatus();
}
