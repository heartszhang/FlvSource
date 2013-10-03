#pragma once
#include <cstdint>
#include <wrl.h>
#include <mfapi.h>
#include "bigendian.hpp"
#include "flv_meta.hpp"
#include "buffer.hpp"
#include "flv.hpp"
#include "avcc.hpp"
#include "MFAsyncCallback.hpp"
#include "MFMediaSourceExt.hpp"
//struct aac_audio_spec_config;// iso-14496-3
//struct aac_raw_frame_data;

struct flv_header{
  uint8_t version   = 0;
  uint8_t has_video = 0;
  uint8_t has_audio = 0;
};
struct tag_header {
  flv::tag_type type;
  int8_t        filter    = 0;    //1 encrypted, 0 : no pre-preocessing
  uint32_t      data_size = 0;    // message size bytes
  uint64_t      nano_timestamp = 0;    //milliseconds
  uint32_t      stream_id = 0;
  uint64_t      data_offset = 0;  // fileposition of payload
};

struct audio_header {
  flv::audio_codec         codec_id;
  flv::sound_rate          sound_rate = flv::sound_rate::_44k;  // kbits
  flv::sound_size          sound_size = flv::sound_size::_16bits; // 8 /16 bits
  flv::sound_type          sound_type = flv::sound_type::stereo;// 1 : stereo, 0 : mono
};

struct video_header {
  flv::frame_type  frame_type;
  flv::video_codec codec_id;
};
struct avc_header{
  flv::avc_packet_type  avc_packet_type;
  uint32_t              composite_time    = 0;
};

// be filled after parse audio-tag header
struct audio_packet_header : public tag_header, public audio_header{
  int32_t                       stream_id;  // Raw stream_id field.  // must be 0
  flv::aac_packet_type          aac_packet_type;// if codec = 10
  packet                        payload;

  audio_packet_header() = default;
  explicit audio_packet_header(tag_header const&t) : tag_header(t){};
  explicit audio_packet_header(tag_header const&t, audio_header const&ah)
      : tag_header(t), stream_id(0), audio_header(ah){
  }

  //载荷数据长度，不包含tag_header, audio_header, aac_packet_type
  uint32_t payload_length()const {
    auto v = data_size - flv::flv_audio_header_length;
    if (codec_id == flv::audio_codec::aac)
      v -= flv::flv_aac_packet_type_length;
    return v;
  }
  uint64_t payload_offset()const{
    return data_offset + data_size - payload_length();
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

struct video_packet_header : public tag_header, public video_header{
  int32_t                          stream_id;       // Raw stream_id field. must be 0
  flv::avc_packet_type             avc_packet_type;// if codec = 7
  uint32_t                         composition_time;// if codec = 7, milli seconds

  packet                            payload;  // not include avc_packet_type and composite_time
//  flv::avcc                         avcc;     //avc_decoder_configuration_record;
  //载荷数据长度，不包含tag_header, audio_header, avc_packet_type
  uint32_t payload_length()const{
    auto v = data_size - flv::flv_video_header_length;
    if (codec_id == flv::video_codec::avc)
      v -= flv::flv_avc_packet_type_length; // sizeof(composite) + sizeof(avcpackettype)
    return v;
  }
  uint64_t payload_offset()const{
    return data_offset + data_size - payload_length();
  }
  video_packet_header() = default;
  explicit video_packet_header(tag_header const&t) : tag_header(t){};
  video_packet_header(tag_header const&t, video_header const&v) :tag_header(t), video_header(v){  }
};

struct flv_file_header : public flv_meta{
  uint64_t            first_media_tag_offset = 0;
  video_packet_header video;
  audio_packet_header audio;
  flv::avcc           avcc;
  struct {
    uint32_t file_header_ready : 1;     // decoded flv header
    uint32_t meta_ready : 1;            // decoded on-meta-data
    uint32_t has_script_data : 1;       // has script data tag
    uint32_t has_video : 1;             // has video tag
    uint32_t has_audio : 1;             // has audio tag
    uint32_t scan_once : 1;             // scanned to end of file
  }status;
};

struct flv_parser : public buffer{
  IMFByteStreamPtr stream;
  HRESULT          skip_previsou_tag_size();

  HRESULT          tag_header(::tag_header*);// parse flv tag header
  HRESULT          flv_header(::flv_header*);
  HRESULT          audio_header(audio_header*rlt);
  HRESULT          aac_packet_type(flv::aac_packet_type*rlt);
  HRESULT          video_header(::video_header*rlt);
  HRESULT          avc_header(::avc_header*);
  HRESULT          on_meta_data(flv_meta*rlt);
  HRESULT          audio_data(packet*);
  HRESULT          video_data(packet*);

  HRESULT begin_flv_header(IMFByteStreamPtr stream, IMFAsyncCallback*, IUnknown*state);
  HRESULT end_flv_header(IMFAsyncResult*, ::flv_header*);

  HRESULT begin_tag_header(int8_t withprevfield, IMFAsyncCallback*, IUnknown*state);
  HRESULT end_tag_header(IMFAsyncResult*, ::tag_header*);

  HRESULT begin_audio_header(IMFAsyncCallback*, IUnknown*);
  HRESULT end_audio_header(IMFAsyncResult*, ::audio_header*);

  HRESULT begin_on_meta_data(uint32_t meta_size, IMFAsyncCallback*, IUnknown*);
  HRESULT end_on_meta_data(IMFAsyncResult*, flv_meta*);

  HRESULT begin_aac_packet_type(IMFAsyncCallback*, IUnknown*);
  HRESULT end_aac_packet_type(IMFAsyncResult*, flv::aac_packet_type *packet_type);

  HRESULT begin_avc_header(IMFAsyncCallback*, IUnknown*);
  HRESULT end_avc_header(IMFAsyncResult*, ::avc_header*);

  HRESULT begin_audio_data(uint32_t payload_length, IMFAsyncCallback*, IUnknown*);
  HRESULT end_audio_data(IMFAsyncResult*, packet*);

  HRESULT begin_video_header(IMFAsyncCallback*,IUnknown*);
  HRESULT end_video_header(IMFAsyncResult*, ::video_header*);

  HRESULT begin_video_data(uint32_t payload_length, IMFAsyncCallback*, IUnknown*);
  HRESULT end_video_data(IMFAsyncResult*, packet*);

protected:
  template<typename data_t> HRESULT end_read(IMFAsyncResult*result, data_t*v);
  template<typename data_t> HRESULT begin_read(IMFAsyncCallback*cb, IUnknown*s, uint32_t length, HRESULT(flv_parser::*decoder)(data_t*));
};

template<typename data_t>
HRESULT flv_parser::begin_read(IMFAsyncCallback*cb,
                               IUnknown*s,
                               uint32_t length,
                               HRESULT(flv_parser::*decoder)(data_t*)){
  reset(length);
  IMFAsyncResultPtr caller_result;
  auto hr = MFCreateAsyncResult(NewMFState<data_t>(data_t()).Get(), cb, s, &caller_result);
  hr = stream->BeginRead(
    current(),
    length,
    MFAsyncCallback::New([this, caller_result, decoder](IMFAsyncResult*result)->HRESULT{
      DWORD cb = 0;
      auto hr = this->stream->EndRead(result, &cb);
      this->move_end(cb);
      if (ok(hr))
        hr = result->GetStatus();
      auto &v = FromAsyncResult<data_t>(caller_result.Get());
      if (ok(hr))
        hr = (this->*decoder)(&v);
      caller_result->SetStatus(hr);
      MFInvokeCallback(caller_result.Get());
      return S_OK;
  }).Get(), nullptr);
  return hr;
}
template<typename data_t>
HRESULT flv_parser::end_read(IMFAsyncResult*result, data_t*v){
  auto const&ah = FromAsyncResult<data_t>(result);
  *v = ah;
  return result->GetStatus();
}
