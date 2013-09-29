#pragma once
#include <cstdint>
#include <string>
#include <cstdint>
#include "bigendian.hpp"
#include "keyframe.hpp"
struct amf_reader : public bigendian::binary_reader{
  int32_t skip_script_data_string();
  int32_t skip_script_data_object();
  int32_t skip_script_data_ecma_array();
  int32_t skip_script_data_strict_array();
  int32_t skip_script_data_value_end(bool *notend);
  int32_t skip_script_data_object_end(bool *notend);
  int32_t skip_script_data_value();
  int32_t skip_script_data_object_property(bool*notend);

  std::string script_data_string();
  uint64_t script_data_value_toui64();
  uint32_t script_data_value_toui32();
  uint8_t script_data_value_toui8();
  double script_data_value_tod();
  keyframes decode_keyframes(int32_t*ret);

  std::vector<uint64_t> strict_uint64_array(bigendian::binary_reader&reader);
  std::vector<double> strict_double_array(bigendian::binary_reader&reader);
  //  explicit amf_reader(bigendian::binary_reader&reader);
  explicit amf_reader(const uint8_t*d, uint32_t len) : binary_reader(d, len){};
  amf_reader() = delete;
};