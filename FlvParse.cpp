// #include "FlvSource.h"
#include "FlvParse.hpp"
#include <cassert>
#include <vector>
#include <utility>
#include <mfapi.h>
#include "asynccallback.hpp"
#include "MFState.hpp"
#include "amf.hpp"

HRESULT flv_parser::flv_header(::flv_header *h){
  auto reader = bigendian::binary_reader(DataPtr(), DataSize());
  reader.skip(3);
  h->version = reader.byte();
  auto l = reader.ui32();
  return l == flv::flv_file_header_length ? S_OK : E_INVALID_PROTOCOL_FORMAT;
}
HRESULT flv_parser::audio_header(::audio_header*v){
  auto x = *reinterpret_cast<raw_audio_tag_header*>(DataPtr());
  v->codec_id = (flv::audio_codec)x.sound_format;
  v->sound_rate = (flv::sound_rate)x.sound_rate;
  v->sound_size = (flv::sound_size)x.sound_size;
  v->sound_type = (flv::sound_type)x.sound_type;
  return S_OK;
}
HRESULT flv_parser::video_header(::video_header*v){
  auto x = *reinterpret_cast<raw_video_tag_header*>(DataPtr());
  v->codec_id = (flv::video_codec)x.codec_id;
  v->frame_type = (flv::frame_type)x.frame_type;
  return S_OK;
}
HRESULT flv_parser::avc_header(::avc_header*v){
  auto reader = bigendian::binary_reader(DataPtr(), DataSize());
  v->avc_packet_type = (flv::avc_packet_type)reader.byte();
  v->composite_time = reader.ui24();
  return S_OK;
}
HRESULT flv_parser::audio_data(packet*p){
  *p = std::move(packet(DataPtr(), DataSize()));
  return S_OK;
}
HRESULT flv_parser::video_data(packet*p){
  *p = std::move(packet(DataPtr(), DataSize()));
  return S_OK;
}
HRESULT flv_parser::aac_packet_type(flv::aac_packet_type*v){
  *v = *reinterpret_cast<flv::aac_packet_type*>(DataPtr());
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

/*
HRESULT flv_parser::begin_flv_header(IMFByteStreamPtr stream, IMFAsyncCallback*cb, IUnknown*s) {
  this->stream = stream;
  Reserve(flv::flv_file_header_length);
  IMFAsyncResultPtr caller_result;
  MFCreateAsyncResult(nullptr, cb, s, &caller_result);
  auto hr = stream->BeginRead(
    DataPtr(), flv::flv_file_header_length,
      MFAsyncCallback::New([this,caller_result](IMFAsyncResult*result)->HRESULT{
          ULONG cb = 0;
          this->stream->EndRead(result,&cb);
          this->MoveEnd(cb);
          auto hr = result->GetStatus();
          if (ok(hr))
            hr = this->check_flv_header(nullptr, nullptr);
          caller_result->SetStatus(hr);
          MFInvokeCallback(caller_result.GetInterfacePtr());
          return S_OK;
        }),
      nullptr);
  return hr;
}
HRESULT flv_parser::end_flv_header(IMFAsyncResult*result, int32_t*rtl){
  *rtl = result->GetStatus();
  return S_OK;
}
*/
HRESULT flv_parser::tag_header(::tag_header*header){
  if(DataSize() < flv::flv_tag_header_length)
    return E_FAIL;
  auto reader = bigendian::binary_reader(DataPtr(), DataSize());
  header->type = flv::tag_type((reader.byte() & 0xf8) >> 3);
  header->timestamp = reader.ui24() + (uint32_t(reader.byte()) <<24);
  // reader.ui24(); stream_id
  return S_OK;
}
uint32_t flv_parser::skip_previsou_tag_size(){
  QWORD cp;
  stream->Seek(MFBYTESTREAM_SEEK_ORIGIN::msoCurrent, sizeof(uint32_t), MFBYTESTREAM_SEEK_FLAG_CANCEL_PENDING_IO, &cp);
  return 0;
}
HRESULT flv_parser::begin_tag_header(int8_t withprevfield, IMFAsyncCallback*cb, IUnknown*s){
  if (withprevfield)
    skip_previsou_tag_size();
  uint32_t len = flv::flv_tag_header_length + withprevfield ? flv::flv_previous_tag_size_field_length : 0;
  return begin_read<::tag_header>(cb, s, len, &flv_parser::tag_header);
}
HRESULT flv_parser::end_tag_header(IMFAsyncResult*result, ::tag_header*v){
  return end_read<::tag_header>(result, v);
}
/*
HRESULT flv_parser::begin_tag_header(int8_t withprevfield, IMFAsyncCallback*cb, IUnknown*s){
  DWORD dlen = flv::flv_tag_header_length + withprevfield ? flv::flv_previous_tag_size_field_length : 0;
  Reset(dlen);  //15
  IMFAsyncResultPtr caller_result;
  auto state = new MFState<::tag_header>(); // refs == 0
  auto hr = MFCreateAsyncResult(state, cb, s, &caller_result);
  if (withprevfield)
    skip_previsou_tag_size();
  hr = stream->BeginRead(
      DataPtr(),
      dlen,
      MFAsyncCallback::New([this, caller_result, withprevfield](IMFAsyncResult*result)->HRESULT{
        DWORD cb = 0;
          auto hr = this->stream->EndRead(result, &cb);
          this->MoveEnd(cb);
          if (ok(hr))
            hr = result->GetStatus();
          auto &tagh = FromAsyncResult<::tag_header>(caller_result.GetInterfacePtr());
          if (ok(hr))
            hr = this->tag_header(&tagh);
          caller_result->SetStatus(hr);
          MFInvokeCallback(caller_result.GetInterfacePtr());
          return S_OK;
        }),
      nullptr
                              );
  return hr;
}

HRESULT flv_parser::end_tag_header(IMFAsyncResult*result, ::tag_header*v){
  auto const&th = FromAsyncResult<::tag_header>(result);
  *v = th;
  return result->GetStatus();
}
*/
HRESULT flv_parser::begin_audio_header(IMFAsyncCallback*cb, IUnknown*s){
  return begin_read<::audio_header>(cb, s, flv::flv_audio_header_length, &flv_parser::audio_header);
}
HRESULT flv_parser::end_audio_header(IMFAsyncResult*result, ::audio_header*v){
  return end_read<::audio_header>(result, v);
}
/*
HRESULT flv_parser::begin_audio_header(IMFAsyncCallback*cb, IUnknown*s){
  DWORD dlen = flv::flv_audio_header_length;
  Reset(dlen);
  IMFAsyncResultPtr caller_result;
  auto hr = MFCreateAsyncResult(new MFState<::audio_header>(), cb, s, &caller_result);
  if(ok(hr))    hr = stream->BeginRead(
    DataPtr(),
    dlen, MFAsyncCallback::New([this, caller_result](IMFAsyncResult*result)->HRESULT{
      DWORD cb = 0;
      auto hr = this->stream->EndRead(result, &cb);
      this->MoveEnd(cb);
      if (ok(hr))
        hr = result->GetStatus();
      auto &ah = FromAsyncResult<::audio_header>(caller_result);
      if (ok(hr))
        hr = this->audio_header(&ah);
      caller_result->SetStatus(hr);
      MFInvokeCallback(caller_result);
      return S_OK;
  }), nullptr);
  return hr;
}
HRESULT flv_parser::end_audio_header(IMFAsyncResult*result, ::audio_header*v){
  auto const&ah = FromAsyncResult<::audio_header>(result);
  *v = ah;
  return result->GetStatus();
}


HRESULT flv_parser::begin_aac_packet_type(IMFAsyncCallback*cb, IUnknown*s){
  DWORD dlen = flv::flv_aac_packet_type_length;
  Reset(dlen);
  IMFAsyncResultPtr caller_result;
  auto hr = MFCreateAsyncResult(new MFState<flv::aac_packet_type>(), cb, s, &caller_result);
  hr = stream->BeginRead(
    DataPtr(), 
    dlen, 
    MFAsyncCallback::New([this, caller_result](IMFAsyncResult*result)->HRESULT{
      DWORD cb = 0;
      auto hr = this->stream->EndRead(result, &cb);
      this->MoveEnd(cb);
      if (ok(hr))
        hr = result->GetStatus();
      auto &t = FromAsyncResult<flv::aac_packet_type>(caller_result);
      if(ok(hr))
        hr = this->aac_packet_type(&t);
      caller_result->SetStatus(hr);
      MFInvokeCallback(caller_result);
      return S_OK;
  }), nullptr);
  return hr;
}
HRESULT flv_parser::end_aac_packet_type(IMFAsyncResult*result, flv::aac_packet_type*v){
  auto t = FromAsyncResult<flv::aac_packet_type>(result);
  *v = t;
  return result->GetStatus();
}
*/
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
/*
HRESULT flv_parser::begin_avc_packet_type(IMFAsyncCallback*cb, IUnknown*s){
  auto dlen = flv::flv_avc_packet_type_length;
  Reset(dlen);
  IMFAsyncResultPtr caller_result;  
  auto hr = MFCreateAsyncResult(new MFState<::avc_header>(), cb, s, &caller_result);
  hr = stream->BeginRead(
    DataPtr(),
    dlen,
    MFAsyncCallback::New([this, caller_result](IMFAsyncResult*result)->HRESULT{
      DWORD cb = 0;
      auto hr = this->stream->EndRead(result, &cb);
      this->MoveEnd(cb);
      auto &ah = FromAsyncResult<::avc_header>(result);
      if (ok(hr))
        hr = result->GetStatus();
      if(ok(hr))
        hr = this->avc_header(&ah);
      caller_result->SetStatus(hr);
      MFInvokeCallback(caller_result);
      return S_OK;
  }), nullptr);
  return hr;
}
HRESULT flv_parser::end_avc_packet_type(IMFAsyncResult*result, flv::avc_packet_type*type, uint32_t*time){
  auto const&h = FromAsyncResult<::avc_header>(result);
  *type = h.avc_packet_type;
  *time = h.composite_time;
  return result->GetStatus();
}
*/
HRESULT read_on_meta_data_value(amf_reader&reader, flv_meta*v){
  auto must_be_ecma_array = reader.byte();
  if (must_be_ecma_array != (uint8_t)script_data_value_type::ecma)
    return E_FAIL;
  HRESULT hr = S_OK;
  reader.ui32();
  for (bool object_not_end = true; object_not_end && ok(hr);){
    auto vname = reader.script_data_string();
    if (vname == "duration"){
      v->duration = reader.script_data_value_toui64();
    }
    else if (vname == "width"){
      v->width = reader.script_data_value_toui32();
    }
    else if (vname == "height"){
      v->height = reader.script_data_value_toui32();
    }
    else if (vname == "videodatarate"){
      v->videodatarate = uint32_t(reader.script_data_value_tod() * 1000);
    }
    else if (vname == "framerate"){
      v->framerate = reader.script_data_value_toui32();
    }
    else if (vname == "videocodecid"){
      v->videocodecid = flv::video_codec(reader.script_data_value_toui32());
    }
    else if (vname == "audiosamplerate"){
      v->audiosamplerate = reader.script_data_value_toui32();
    }
    else if (vname == "audiosamplesize"){
      v->audiosamplesize = reader.script_data_value_toui32();
    }
    else if (vname == "stereo"){
      v->stereo = reader.script_data_value_toui8();
    }
    else if (vname == "audiocodecid"){
      v->audiocodecid = flv::audio_codec(reader.script_data_value_toui32());
    }
    else if (vname == "filesize"){
      v->filesize = reader.script_data_value_toui64();
    }
    else if (vname == "keyframes"){
      v->keyframes = std::move(reader.decode_keyframes((int32_t*)&hr));
    }
    else if (vname == ""){
      hr = reader.skip_script_data_value_end(&object_not_end);
    }
    else{
      hr = reader.skip_script_data_value();
    }
  }
  return hr;
}

HRESULT flv_parser::on_meta_data(flv_meta*v) {
  auto reader = amf_reader(DataPtr(), DataSize());
  auto hr = reader.skip_script_data_value();
  if (ok(hr))
    hr = read_on_meta_data_value(reader,v);
  return hr;
}
HRESULT flv_parser::begin_on_meta_data(uint32_t meta_size, IMFAsyncCallback*cb, IUnknown*s){
  return begin_read<flv_meta>(cb, s, meta_size, &flv_parser::on_meta_data);
}
HRESULT flv_parser::end_on_meta_data(IMFAsyncResult*result, flv_meta*v){
  return end_read<flv_meta>(result, v);
}
/*
HRESULT flv_parser::begin_on_meta_data(uint32_t meta_size, IMFAsyncCallback*cb, IUnknown*s){
  Reset(meta_size);
  IMFAsyncResultPtr caller_result;
  auto hr = MFCreateAsyncResult(new MFState<flv_meta>(), cb, s, &caller_result);
  hr = stream->BeginRead(
    DataPtr(), meta_size,
    MFAsyncCallback::New([this, caller_result, meta_size](IMFAsyncResult*result)->HRESULT{
          DWORD cb = 0;
          auto hr = this->stream->EndRead(result, &cb);
          this->MoveEnd(cb);
          if (ok(hr))
            hr = result->GetStatus();
          auto &m = FromAsyncResult<flv_meta>(caller_result.GetInterfacePtr());
          if (ok(hr))
            hr = this->on_meta_data(meta_size, &m);
          caller_result->SetStatus(hr);
          MFInvokeCallback(caller_result.GetInterfacePtr());
          return S_OK;
        }), nullptr);
  return hr;
}
HRESULT flv_parser::end_on_meta_data(IMFAsyncResult*result, flv_meta*v){
  auto m = FromAsyncResult<flv_meta>(result);
  *v = m;
  return result->GetStatus();
}
*/
