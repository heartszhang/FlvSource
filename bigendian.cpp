#include "bigendian.hpp"
#include <utility>
#include <WinSock2.h>  // htond/ ntohd
namespace bigendian{
uint64_t toui64(const uint8_t *input)
{
  return ntohll(*(const uint64_t*)input);
  /*
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
  */
}
uint32_t touint32(uint8_t const*input){
  return ntohl(*(const u_long*)input);
  /*
  uint32_t r;
  uint8_t*data = (uint8_t*)&r;
  data[0] = input[3];
  data[1] = input[2];
  data[2] = input[1];
  data[3] = input[0];
  return r;
  */
}
uint8_t binary_reader::byte(){
  auto v = data[pointer++];
  return v;
}
uint16_t binary_reader::ui16(){
  auto v= ntohs(*(const u_short*)(data + pointer));
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
::packet binary_reader::packet(uint32_t l){
  auto v = ::packet(l);
  memcpy_s(v._, v.length, data+pointer, l);
  pointer += l;
  return std::move(v);
}

void binary_writer::ui32(uint32_t v){
  const uint8_t *p = reinterpret_cast<const uint8_t*>(&v);
  data[pointer]     = p[3];
  data[pointer + 1] = p[2];
  data[pointer + 2] = p[1];
  data[pointer + 3] = p[0];
  pointer += sizeof(uint32_t);
}
void binary_writer::ui24(uint32_t v){
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
  data[pointer]     = p[2];
  data[pointer + 1] = p[1];
  data[pointer + 2] = p[0];
  pointer += 3;
}
void binary_writer::ui16(uint16_t v){
  const uint8_t*p = reinterpret_cast<const uint8_t*>(&v);
  data[pointer] = p[1];
  data[pointer + 1] = p[0];
  pointer += sizeof(uint16_t);
}
void binary_writer::packet(::packet const&v){
  memcpy_s(data + pointer, length - pointer, v._, v.length);
  pointer += v.length;
}
}