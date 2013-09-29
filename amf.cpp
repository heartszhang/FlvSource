#include "amf.hpp"
#include "flv.hpp"
int32_t amf_reader::skip_script_data_value(){
  auto t = byte();
  int32_t hr = 0;
  switch (script_data_value_type(t)){
  case script_data_value_type::number:
  skip(sizeof(double));
  break;
  case script_data_value_type::boolean:
  skip(sizeof(uint8_t));
  break;
  case script_data_value_type::string:
  hr = skip_script_data_string();
  break;
  case script_data_value_type::object:
  hr = skip_script_data_object();
  break;
  case script_data_value_type::movie_clip:
  hr = skip_script_data_string();
  break;
  case script_data_value_type::null:
  break;
  case script_data_value_type::undefined:
  break;
  case script_data_value_type::reference:
  skip(sizeof(uint16_t));
  break;
  case script_data_value_type::ecma:
  hr = skip_script_data_ecma_array();
  break;
  case script_data_value_type::object_end_marker:
  break;
  case script_data_value_type::array:
  hr = skip_script_data_strict_array();
  break;
  case script_data_value_type::date:
  skip(sizeof(double));
  skip(sizeof(uint16_t));
  break;
  case script_data_value_type::long_string:
  hr = skip_script_data_string();
  break;
  default:
  hr = -1;
  }
  return hr;
}
keyframes amf_reader::decode_keyframes(int32_t*ret){
  keyframes va;
  *ret = 0;
  // ecma-array
  auto t = byte();
  if (t != (uint8_t)script_data_value_type::object)
  {
    *ret = -1;
    return std::move(va);
  }
  for (bool neop = true; neop && *ret == 0;){
    auto v = script_data_string();
    if (v == "filepositions"){
      byte();  // strict array type = 10
      auto cnt = ui32();
      for (uint32_t i = 0; i < cnt; ++i){
        va.push_fileposition(script_data_value_toui64());
      }
    }
    else if (v == "times"){
      byte();
      auto cnt = ui32();
      for (uint32_t i = 0; i < cnt; ++i){
        va.push_time(script_data_value_tod());
      }
    }
    else if (v == ""){
      *ret = skip_script_data_object_end(&neop);
    }
    else{
      *ret = skip_script_data_value();
    }
  }
  return va;
}
int32_t amf_reader::skip_script_data_object(){
  int32_t hr = 0;
  for (auto notend = true; notend && hr == 0;){
    hr = skip_script_data_object_property(&notend);
  }
  return hr;
}
int32_t amf_reader::skip_script_data_object_property(bool*notend){
  auto v = script_data_string();
  int32_t hr = 0;
  if (v.empty()){
    auto x = byte();
    if (x == 0x9){
      notend = false;
      return 0;
    }
    else
      return -1;
  }
  return skip_script_data_value();
}

int32_t amf_reader::skip_script_data_ecma_array(){
  auto l = ui32();
  int32_t hr = 0;
  for (bool notend = true; notend && hr == 0;){
    hr = skip_script_data_object_property(&notend);
  }
  return hr;
}
int32_t amf_reader::skip_script_data_strict_array(){
  auto l = ui32();
  int32_t hr = 0;
  for (uint32_t i = 0; i < l && hr == 0; ++i){
    hr = skip_script_data_value();
  }
  return hr;
}
int32_t amf_reader::skip_script_data_string(){
  auto l = ui16();
  skip(l);
  return 0;
}
int32_t amf_reader::skip_script_data_object_end(bool*notend){
  auto v = byte();
  *notend = false;
  return  (v == (uint8_t)script_data_value_type::object_end_marker) ? 0 : -1;
}
int32_t amf_reader::skip_script_data_value_end(bool *notend){
  return skip_script_data_object_end(notend);
}
std::string amf_reader::script_data_string(){
  auto l = ui16();
  std::string v((const char*)data + pointer, l);
  skip(l);
  return v;
}
uint64_t amf_reader::script_data_value_toui64(){
  byte();  // type
  auto v = numberic();
  return static_cast<uint64_t>(v);
}

double amf_reader::script_data_value_tod(){
  byte();
  return numberic();
}

uint8_t amf_reader::script_data_value_toui8(){
  byte(); // type
  return byte();
}

uint32_t amf_reader::script_data_value_toui32(){
  byte();
  auto v = numberic();
  return static_cast<uint32_t>(v);
}