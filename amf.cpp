#include "amf.hpp"
#include <cassert>
#include "flv.hpp"
#include "flv_meta.hpp"
int32_t flv::amf_reader::skip_script_data_value(){
  auto t = byte();
  int32_t hr = 0;
  switch (flv::script_data_value_type(t)){
  case flv::script_data_value_type::number:
  skip(sizeof(double));
  break;
  case flv::script_data_value_type::boolean:
  skip(sizeof(uint8_t));
  break;
  case flv::script_data_value_type::string:
  hr = skip_script_data_string();
  break;
  case flv::script_data_value_type::object:
  hr = skip_script_data_object();
  break;
  case flv::script_data_value_type::movie_clip:
  hr = skip_script_data_string();
  break;
  case flv::script_data_value_type::null:
  break;
  case flv::script_data_value_type::undefined:
  break;
  case flv::script_data_value_type::reference:
  skip(sizeof(uint16_t));
  break;
  case flv::script_data_value_type::ecma:
  hr = skip_script_data_ecma_array();
  break;
  case flv::script_data_value_type::object_end_marker:
  break;
  case flv::script_data_value_type::array:
  hr = skip_script_data_strict_array();
  break;
  case flv::script_data_value_type::date:
  skip(sizeof(double));
  skip(sizeof(uint16_t));
  break;
  case flv::script_data_value_type::long_string:
  hr = skip_script_data_string();
  break;
  default:
  hr = -1;
  }
  return hr;
}

// only accept fields filepositions and times
// other fields would be ignored
keyframes flv::keyframes_decoder::decode(flv::amf_reader &reader, int32_t*ret){
  keyframes va;
  *ret = 0;
  
  auto t = reader.byte(); // object
  assert(t == (uint8_t)flv::script_data_value_type::object);
  for (bool neop = true; neop && *ret == 0;){
    auto v = reader.script_data_string();
    if (v == "filepositions"){
      reader.byte();  // strict array type = 10
      auto cnt = reader.ui32();
      for (uint32_t i = 0; i < cnt; ++i){
        va.push_fileposition(reader.script_data_value_toui64());
      }
    }
    else if (v == "times"){
      reader.byte();
      auto cnt = reader.ui32();
      for (uint32_t i = 0; i < cnt; ++i){
        va.push_time(reader.script_data_value_tod());
      }
    }
    else if (v == ""){
      *ret = reader.skip_script_data_object_end(&neop);
    }
    else{
      *ret = reader.skip_script_data_value();
    }
  }
  return std::move(va);
}
int32_t flv::amf_reader::skip_script_data_object(){
  int32_t hr = 0;
  for (auto notend = true; notend && hr == 0;){
    hr = skip_script_data_object_property(&notend);
  }
  return hr;
}

// treat object-end-marker as a leagal property
// terminated by object-end-marker
int32_t flv::amf_reader::skip_script_data_object_property(bool*notend){
  auto v = script_data_string();
  if (v.empty()){
    auto x = byte();
    if (x == (uint8_t)flv::script_data_value_type::object_end_marker){
      notend = false;
      return 0;
    }
    else
      return -1;
  }
  return skip_script_data_value();
}

// ecma's terminated by object-end-marker({0, 0, 9})
int32_t flv::amf_reader::skip_script_data_ecma_array(){
  ui32();
  int32_t hr = 0;
  for (bool notend = true; notend && hr == 0;){
    hr = skip_script_data_object_property(&notend);
  }
  return hr;
}

//script_data strict array
int32_t flv::amf_reader::skip_script_data_strict_array(){
  auto l = ui32();
  int32_t hr = 0;
  for (uint32_t i = 0; i < l && hr == 0; ++i){
    hr = skip_script_data_value();
  }
  return hr;
}

// script_data_string, no data_value type field
int32_t flv::amf_reader::skip_script_data_string(){
  auto l = ui16();
  skip(l);
  return 0;
}

// *notend = false
// return -1 if 0-0 followed by not 9
int32_t flv::amf_reader::skip_script_data_object_end(bool*notend){
  auto v = byte();
  *notend = false;
  return  (v == (uint8_t)flv::script_data_value_type::object_end_marker) ? 0 : -1;
}

// script_data_value_end, {0, 0, 9}, {0, 0} has been treated as name's length
int32_t flv::amf_reader::skip_script_data_value_end(bool *notend){
  return skip_script_data_object_end(notend);
}

// script_data_value_string, without type-field
std::string flv::amf_reader::script_data_string(){
  auto l = ui16();  // length
  std::string v((const char*)data + pointer, l);
  skip(l);
  return v;
}

//script_data_value_number to uint64_t;
uint64_t flv::amf_reader::script_data_value_toui64(){
  byte();  // script_data_value_type::number
  auto v = numberic();
  return static_cast<uint64_t>(v);
}

// script_data_value_number
double flv::amf_reader::script_data_value_tod(){
  byte();
  return numberic();
}

// script_data_value_boolean to uint8_t
uint8_t flv::amf_reader::script_data_value_toui8(){
  byte(); // script_data_value_type::boolean
  return byte();
}

// script_data_value_number to uint32_t
uint32_t flv::amf_reader::script_data_value_toui32(){
  byte();
  auto v = numberic();
  return static_cast<uint32_t>(v);
}

const static uint32_t e_fail = 0x80004005L;

uint32_t flv::on_meta_data_decoder::decode(flv::amf_reader &reader, flv_meta*v){
  auto must_be_ecma_array = reader.byte();
  if (must_be_ecma_array != (uint8_t)flv::script_data_value_type::ecma)
    return e_fail;
  uint32_t hr = 0;
  reader.ui32(); // ecma
  for (bool object_not_end = true; object_not_end && hr == 0;){
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
      v->videodatarate = reader.script_data_value_toui32() * 1000;
    }
    else if (vname == "framerate"){
      v->framerate = reader.script_data_value_toui32();
    }
    else if (vname == "videocodecid"){
      v->videocodecid = flv::video_codec(reader.script_data_value_toui32());
    }
    else if (vname == "audiosamplerate"){
      auto x = reader.script_data_value_toui32();
      if (x < 4){
        v->audiosamplerate = 44100 * (1 << x) / 8;
      }
      else v->audiosamplerate = x;
    }
    else if (vname == "audiosamplesize"){
      v->audiosamplesize = (uint16_t)reader.script_data_value_toui32();
    }
    else if (vname == "audiodatarate"){
      v->audiodatarate = reader.script_data_value_toui32() * 1000;
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
    else if (vname == "datasize"){
      v->datasize = reader.script_data_value_toui64();
    }
    else if (vname == "keyframes"){
      v->keyframes = std::move(flv::keyframes_decoder().decode(reader, (int32_t*)&hr));
    }
    else if (vname == "hasAudio"){
      v->has_audio = reader.script_data_value_toui8();
    }
    else if (vname == "hasVideo"){
      v->has_video = reader.script_data_value_toui8();
    }
    else if (vname == "hasMetadata"){
      v->has_metadata = reader.script_data_value_toui8();
    }
    else if (vname == "canSeekToEnd"){
      v->can_seek_to_end = reader.script_data_value_toui8();
    }
    else if (vname == "lasttimestamp"){
      v->last_timestamp = reader.script_data_value_toui32();
    }
    else if (vname == "lastkeyframetimestamp"){
      v->last_keyframe_timestamp = reader.script_data_value_toui32();
    }
    else if (vname == "audiosize"){
      v->audiosize = reader.script_data_value_toui32();
    }
    else if (vname == "audiodelay"){
      v->audiodelay = reader.script_data_value_toui32();
    }
    else if (vname == ""){
      hr = reader.skip_script_data_value_end(&object_not_end);
    }
    else{
      hr = reader.skip_script_data_value();
    }
  }
  assert(reader.pointer == reader.length);
  return hr;
}
