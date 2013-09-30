#include "avcc.hpp"
#include <cassert>

//startcode + sps[0] + startcode + pps
packet flv::avcc::code_private_data()const{  // sequence_header
  assert(!sps.empty() && !pps.empty());
  uint32_t startcode = 0x00000001;
  auto l = nal + sps[0].length + nal + pps[0].length;
  packet v(l);
  bigendian::binary_writer writer(v._, v.length);
  nal != 4 ? writer.ui24(startcode) : writer.ui32(startcode);
  writer.packet(sps[0]);
  nal != 4 ? writer.ui24(startcode) : writer.ui32(startcode);
  writer.packet(pps[0]);
  return std::move(v);
}

// uint16_be(sps_length) + sps + uint16_be(pps_length) + pps
packet flv::avcc::sequence_header()const{
  auto l = sizeof(uint16_t)+sps[0].length + sizeof(uint16_t)+pps[0].length;
  packet v(l);
  bigendian::binary_writer writer(v._, v.length);
  writer.ui16(static_cast<uint16_t>(sps[0].length));
  writer.packet(sps[0]);
  writer.ui16(static_cast<uint16_t>(pps[0].length));
  writer.packet(pps[0]);
  return std::move(v);
}
flv::avcc flv::avcc_reader::avcc(){
  flv::avcc v;
  byte();  // version;
  v.profile = byte();
  byte();  // profile-compatibility
  v.level = byte();
  v.nal = (byte() & 0x03) + 1;
  auto sps_count = byte() & 0x1f;
  for (uint8_t i = 0; i < sps_count; ++i){
    auto l = ui16();
    v.sps.push_back(std::move(this->packet(l)));
  }
  auto pps_count = byte();
  for (uint8_t i = 0; i < pps_count; ++i){
    auto l = ui16();
    v.pps.push_back(std::move(this->packet(l)));
  }
  assert(pointer == length);
  return std::move(v);
}

::packet flv::nalu_reader::nalu(){
  ::packet v;
  if (pointer >= length)
    return v;
  auto l = ui32();  //nalu length
  return packet(l);
}
