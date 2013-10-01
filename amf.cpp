#include "amf.hpp"
#include <cassert>
#include "flv.hpp"
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