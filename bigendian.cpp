#include "bigendian.hpp"
#include <WinSock2.h>  // htond/ ntohd
namespace bigendian{
  uint64_t toui64(const uint8_t *input)
{
  uint64_t rval;
  uint8_t *data = (uint8_t *)&rval;

  data[0] = input[7];
  data[1] = input[6];
  data[2] = input[5];
  data[3] = input[4];
  data[4] = input[3];
  data[5] = input[2];
  data[6] = input[1];
  data[7] = input[0];

  return rval;
}
uint32_t touint32(uint8_t const*input){
  uint32_t r;
  uint8_t*data = (uint8_t*)&r;
  data[0] = input[3];
  data[1] = input[2];
  data[2] = input[1];
  data[3] = input[0];
  return r;
}
uint8_t binary_reader::byte(){
  auto v = data[pointer++];
  return v;
}
uint16_t binary_reader::ui16(){
  uint16_t v = uint16_t(data[pointer + 1]) | (uint16_t(data[pointer]) << 8);
  pointer += sizeof(uint16_t);
  return v;
}
uint32_t binary_reader::ui32(){
  auto v = touint32(data + pointer);
  pointer += sizeof(uint32_t);
  return v;
}
uint64_t binary_reader::ui64(){
  auto v = toui64(data + pointer);
  pointer += sizeof(uint64_t);
  return v;
}
uint32_t binary_reader::ui24(){
  auto v = (uint32_t(data[pointer] << 16)) | (uint32_t(data[pointer + 1]) << 8) | (uint32_t(data[pointer + 2]));
  pointer += 3;// ui24
  return v;
}
void binary_reader::skip(uint32_t bytes){
  pointer += bytes;
}
double binary_reader::numberic(){
  const uint64_t* lv = reinterpret_cast<const uint64_t*>(data + pointer);
  auto v = ntohd(*lv);
  pointer += sizeof(uint64_t);
  return v;
}
}