#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <comip.h>
#include <mfidl.h>
#include <mfapi.h>
#include "bigendian.hpp"
#include "flv_meta.hpp"
#include "buffer.hpp"
#include "flv.hpp"


struct aac_audio_spec_config;
struct aac_raw_frame_data;
struct tag_header {
  flv::tag_type type;
  int8_t        filter;  //1 encrypted, 0 : no pre-preocessing
  uint32_t      data_size;  // message size bytes
  uint32_t      timestamp;  //milliseconds
  //with out audio/ video/ script tag header
  uint64_t data_offset;
  uint64_t nano_time()const{
    return 10000 * uint64_t(timestamp);
  }
};

// be filled after parse audio-tag header
struct audio_stream_header {
  int32_t                       stream_id;  // Raw stream_id field.  // must be 0
  flv::audio_codec              codec_id;  // 10 = AAC
  flv::sound_rate               sound_rate;  // 0: 5.5k, 1: 11k, 2: 22k, 3:44k
  flv::sound_size               sound_size; // 0 : 8bit, 1 : 16bit
  flv::sound_type               sound_type;  // 0: Mono, 1: stereo
// if codec = 10, 0: aac-sequence header, 1: aac raw, 2 : AVC end ofsequence
  flv::aac_packet_type          aac_packet_type;
  uint64_t                      data_offset;// file position absolute
  tag_header tag;
  audio_stream_header() = default;
  explicit audio_stream_header(tag_header const&t) : tag(t){};
  explicit audio_stream_header(tag_header const&t, audio_header const&ah) : tag(t){}
  uint32_t payload_length()const {
    auto v = tag.data_size - flv::flv_tag_size - flv::audio_header_size;
    if (codec_id == flv::audio_codec::aac)
      --v;
    return v;
  }
};


struct avc_decoder_configuration_record;
struct avc_nalus;
// be filled after parse video-tag
struct video_stream_header {
  int32_t                          stream_id;  // Raw stream_id field. must be 0
  flv::frame_type                  frame_type;
  flv::video_codec                 codec_id;  // 7 = avc
 // if codec = 7,  0: avc-sequence header, 1 : nalu, 2 : avc end of sequence
  flv::avc_packet_type             avc_packet_type;
  // if codec = 7, avc_packet_type = 1; else 0
  int32_t                          composition_time;
  //  avc_decoder_configuration_record *private_data;//IF AVCPacketType == 0
  //  avc_nalus                        *nalus;//AVCPacketType == 1
  uint64_t                         data_offset;
  tag_header tag;
  uint32_t payload_length()const{
    auto v = tag.data_size - flv::flv_tag_size - flv::video_header_size;
    if (codec_id == flv::video_codec::avc)
      v -= 4; // sizeof(composite) + sizeof(avcpackettype)
    return v;
  }
  video_stream_header() = default;
  explicit video_stream_header(tag_header const&t) : tag(t){};
 video_stream_header(tag_header const&t, video_header const&v):tag(t){}
};

enum class parser_result : int32_t {
  ok = 0,
    // any positive value means how many bytes needed
    failed = -1,
};

struct audio_header {
 flv::audio_codec codec_id;
 flv::sound_rate          sound_rate;  // kbits
 flv::sound_size           sound_size; // 8 /16 bits
 flv::sound_type           stereo;// 1 : stereo, 0 : mono
};
struct audio_packet{

};
// AudioSpecificConfig
struct audio_specific_config{

};

struct video_header {
  flv::frame_type frame_type;
  flv::video_codec codec_id;
};
struct video_packet{

};
// be filled after read the first script tag
// FlvStreamHeader
// Holds information from the system header.
struct flv_file_header{
  std::unique_ptr<audio_stream_header> audio;
  std::unique_ptr<video_stream_header> video;
  flv_meta meta;
  uint64_t first_media_tag_offset;
//  std::vector<tag_header> tags;
  struct {
    uint32_t file_header_ready : 1;
    uint32_t meta_ready : 1;
    uint32_t has_script_data : 1;
    uint32_t has_video : 1;
    uint32_t has_audio : 1;
    uint32_t scan_once : 1;
  }status;
};

typedef _com_ptr_t < _com_IIID<IMFMediaEventQueue, &__uuidof(IMFMediaEventQueue)>> IMFMediaEventQueuePtr;
typedef _com_ptr_t < _com_IIID < IMFPresentationDescriptor, &_uuidof(IMFPresentationDescriptor)>> IMFPresentationDescriptorPtr;
typedef _com_ptr_t < _com_IIID < IMFByteStream, &_uuidof(IMFByteStream)>> IMFByteStreamPtr;
typedef _com_ptr_t < _com_IIID < IMFMediaStream, &_uuidof(IMFMediaStream)>> IMFMediaStreamPtr;
typedef _com_ptr_t < _com_IIID<IMFMediaType, &__uuidof(IMFMediaType) >> IMFMediaTypePtr;
typedef _com_ptr_t < _com_IIID<IMFStreamDescriptor, &__uuidof(IMFStreamDescriptor) >> IMFStreamDescriptorPtr;
typedef _com_ptr_t < _com_IIID<IMFSample, &__uuidof(IMFSample) >> IMFSamplePtr;
typedef _com_ptr_t < _com_IIID<IUnknown, &__uuidof(IUnknown) >> IUnknownPtr;
typedef _com_ptr_t < _com_IIID<IMFMediaBuffer, &__uuidof(IMFMediaBuffer) >> IMFMediaBufferPtr;

struct flv_parser : public buffer{
  IMFByteStreamPtr        stream;
  int8_t                           signature_passed;
  uint32_t                         previsou_tag_size();

  // parse flv file header
  // ok, fail/ or more data; return 0: passed, -1:error
  int32_t                          check_flv_header();

  // parse flv tag header
  HRESULT                       tag_header(tag_header*);
  audio_header                     audio_tag_header(int32_t*rlt);
  audio_specific_config            audio_specific_config(int32_t*rlt);
  audio_packet                     aac_audio_packet(int32_t*rtl);
  int32_t                          aac_packet_type(int32_t*rlt);
  video_header                     video_tag_header(int32_t*rlt);
  int32_t                          avc_packet_type(int32_t*rlt);
  int32_t                          avc_composition_time(int32_t*rlt);
  avc_decoder_configuration_record avc_decoder_configuration_record(int32_t*rlt);
  video_packet                     avc_video_packet(int32_t*rlt);
  HRESULT                         on_meta_data(flv_meta*rlt);

  HRESULT begin_check_flv_header(IMFByteStreamPtr stream, IMFAsyncCallback*, IUnknown*state);
  HRESULT end_check_flv_header(IMFAsyncResult*, int32_t*);

  HRESULT begin_tag_header(int8_t withprevfield, IMFAsyncCallback*, IUnknown*state);
  HRESULT end_tag_header(IMFAsyncResult*, ::tag_header*);

  HRESULT begin_audio_header(IMFAsyncCallback*, IUnknown*);
  HRESULT end_audio_header(IMFAsyncResult*, audio_header*);

  HRESULT begin_audio_specific_config(IMFAsyncCallback*, IUnknown*);
  HRESULT end_audio_specific_config(IMFAsyncResult*, ::audio_specific_config*);

  HRESULT begin_on_meta_data(uint32_t meta_size, IMFAsyncCallback*, IUnknown*);
  HRESULT end_on_meta_data(IMFAsyncResult*, flv_meta*);

  HRESULT begin_aac_packet_type(IMFAsyncCallback*, IUnknown*);
  HRESULT end_aac_packet_type(IMFAsyncResult*, flv::aac_packet_type *packet_type);

  HRESULT begin_avc_packet_type(IMFAsyncCallback*, IUnknown*);
  HRESULT end_avc_packet_type(IMFAsyncResult*, flv::avc_packet_type *, uint32_t &copotime);

  HRESULT begin_audio_data(uint32_t payload_length, IMFAsyncCallback*, IUnknown*);
  HRESULT end_audio_data(IMFAsyncResult*, uint8_t*buf, uint32_t buflen, uint32_t*cb);

  HRESULT begin_video_header(IMFAsyncCallback*,IUnknown*);
  HRESULT end_video_header(IMFAsyncResult*, video_header*);

  HRESULT begin_video_data(uint32_t payload_length, IMFAsyncCallback*, IUnknown*);
  HRESULT end_video_data(IMFAsyncResult*, uint8_t*buf, uint32_t buflen, uint32*cb);
};
