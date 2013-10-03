#include "FlvParse.hpp"
#include <cassert>
#include <vector>
#include <utility>
#include <mfapi.h>
#include "asynccallback.hpp"
#include "MFState.hpp"
#include "amf.hpp"

HRESULT flv_parser::flv_header(::flv_header *h){
  auto reader = bigendian::binary_reader(current(), size());
  reader.skip(3);
  h->version = reader.byte();
  auto f = reader.byte();
  if (f & flv::flv_file_header_video_mask)
    h->has_video = 1;
  if (f & flv::flv_file_header_audio_mask)
    h->has_audio = 1;
  auto l = reader.ui32();
  assert(reader.pointer == reader.length);
  return l == flv::flv_file_header_length ? S_OK : E_INVALID_PROTOCOL_FORMAT;
}
HRESULT flv_parser::audio_header(::audio_header*v){
  auto x = *reinterpret_cast<raw_audio_tag_header*>(current());
  v->codec_id = (flv::audio_codec)x.sound_format;
  v->sound_rate = (flv::sound_rate)x.sound_rate;
  v->sound_size = (flv::sound_size)x.sound_size;
  v->sound_type = (flv::sound_type)x.sound_type;
  assert(size() == sizeof(raw_audio_tag_header));
  return S_OK;
}
HRESULT flv_parser::video_header(::video_header*v){
  auto x = *reinterpret_cast<raw_video_tag_header*>(current());
  v->codec_id = (flv::video_codec)x.codec_id;
  v->frame_type = (flv::frame_type)x.frame_type;
  assert(size() == sizeof(raw_video_tag_header));
  return S_OK;
}
HRESULT flv_parser::avc_header(::avc_header*v){
  auto reader = bigendian::binary_reader(current(), size());
  v->avc_packet_type = (flv::avc_packet_type)reader.byte();
  v->composite_time = reader.ui24();
  assert(reader.pointer == reader.length);
  return S_OK;
}
HRESULT flv_parser::audio_data(packet*p){
  *p = std::move(packet(current(), size()));
  return S_OK;
}
HRESULT flv_parser::video_data(packet*p){
  *p = std::move(packet(current(), size()));
  return S_OK;
}
HRESULT flv_parser::aac_packet_type(flv::aac_packet_type*v){
  *v = *reinterpret_cast<flv::aac_packet_type*>(current());
  return S_OK;
}

HRESULT flv_parser::begin_flv_header(IMFByteStreamPtr stream, IMFAsyncCallback*cb, IUnknown*s) {
  this->stream = stream;
  return begin_read<::flv_header>(cb, s, flv::flv_file_header_length, &flv_parser::flv_header);
}
HRESULT flv_parser::end_flv_header(IMFAsyncResult*result, ::flv_header*v){
  return end_read<::flv_header>(result, v);
}
HRESULT flv_parser::begin_audio_data(uint32_t length, IMFAsyncCallback*cb, IUnknown*s){
  return begin_read<packet>(cb, s, length, &flv_parser::audio_data);
}
HRESULT flv_parser::end_audio_data(IMFAsyncResult*result, packet*v){
  return end_read<packet>(result, v);
}
HRESULT flv_parser::begin_video_header(IMFAsyncCallback*cb, IUnknown*s){
  return begin_read<::video_header>(cb, s, flv::flv_video_header_length, &flv_parser::video_header);
}
HRESULT flv_parser::end_video_header(IMFAsyncResult*result, ::video_header*v){
  return end_read<::video_header>(result, v);
}
HRESULT flv_parser::begin_video_data(uint32_t length, IMFAsyncCallback*cb, IUnknown*s){
  return begin_read<packet>(cb, s, length, &flv_parser::video_data);
}
HRESULT flv_parser::end_video_data(IMFAsyncResult*result, packet*v){
  return end_read<packet>(result, v);
}

HRESULT flv_parser::tag_header(::tag_header*header){
  auto reader = bigendian::binary_reader(current(), size());
  if (reader.length == 0){
    header->type = flv::tag_type::eof;
    return S_OK;
  }
  auto f = reader.byte();
  header->type = flv::tag_type(f &  flv::flv_tag_header_type_mask);
  header->filter = (f & (1 << flv::flv_tag_header_filter_mask)) >> flv::flv_tag_header_filter_mask;
  header->data_size = reader.ui24();
  header->nano_timestamp = uint64_t(reader.ui24() + (uint32_t(reader.byte()) << 24)) * 10000;  // millis to nano seconds
  
  header->stream_id =  reader.ui24(); 
  stream->GetCurrentPosition(&header->data_offset);
  assert(reader.pointer == reader.length);
  return S_OK;
}
HRESULT flv_parser::skip_previsou_tag_size(){
  QWORD cp;
  stream->Seek(MFBYTESTREAM_SEEK_ORIGIN::msoCurrent, sizeof(uint32_t), MFBYTESTREAM_SEEK_FLAG_CANCEL_PENDING_IO, &cp);
  return S_OK;
}
HRESULT flv_parser::begin_tag_header(int8_t withprevfield, IMFAsyncCallback*cb, IUnknown*s){
  if (withprevfield)
    skip_previsou_tag_size();
  return begin_read<::tag_header>(cb, s, flv::flv_tag_header_length, &flv_parser::tag_header);
}
HRESULT flv_parser::end_tag_header(IMFAsyncResult*result, ::tag_header*v){
  return end_read<::tag_header>(result, v);
}
HRESULT flv_parser::begin_audio_header(IMFAsyncCallback*cb, IUnknown*s){
  return begin_read<::audio_header>(cb, s, flv::flv_audio_header_length, &flv_parser::audio_header);
}
HRESULT flv_parser::end_audio_header(IMFAsyncResult*result, ::audio_header*v){
  return end_read<::audio_header>(result, v);
}
HRESULT flv_parser::begin_aac_packet_type(IMFAsyncCallback*cb, IUnknown*s){
  return begin_read<flv::aac_packet_type>(cb, s, flv::flv_aac_packet_type_length, &flv_parser::aac_packet_type);
}
HRESULT flv_parser::end_aac_packet_type(IMFAsyncResult*result, flv::aac_packet_type*v){
  return end_read<flv::aac_packet_type>(result, v);
}
HRESULT flv_parser::begin_avc_header(IMFAsyncCallback*cb, IUnknown*s){
  return begin_read<::avc_header>(cb, s, flv::flv_avc_packet_type_length, &flv_parser::avc_header);
}
HRESULT flv_parser::end_avc_header(IMFAsyncResult*result, ::avc_header*v){
  return end_read<::avc_header>(result, v);
}

HRESULT flv_parser::on_meta_data(flv_meta*v) {
  auto reader = flv::amf_reader(current(), size());
  auto hr = reader.skip_script_data_value();
  if (ok(hr))
    hr = flv::on_meta_data_decoder().decode(reader, v);
  return hr;
}
HRESULT flv_parser::begin_on_meta_data(uint32_t meta_size, IMFAsyncCallback*cb, IUnknown*s){
  return begin_read<flv_meta>(cb, s, meta_size, &flv_parser::on_meta_data);
}
HRESULT flv_parser::end_on_meta_data(IMFAsyncResult*result, flv_meta*v){
  return end_read<flv_meta>(result, v);
}
