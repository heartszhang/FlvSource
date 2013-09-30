#include "avcc.hpp"

packet flv::avcc::code_private_data()const{  // sequence_header
  assert(sps_count > 0 && pps_count > 0);
  uint32_t startcode = 0x00000001;
  //startcode + sps[0] + startcode + pps
  auto l = nal + sps[0].length + nal + pps[0].length;
  auto v = packet(l);
  bigendian::binary_writer writer(v._, v.length);
  nal < 4 ? writer.ui24(startcode) : writer.ui32(startcode);
  writer.packet(sps[0]);
  nal < 4 ? writer.ui24(startcode) : writer.ui32(startcode);
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
  v.sps_count = byte() & 0x1f;
  for (uint8_t i = 0; i < v.sps_count; ++i){
    auto l = ui16();
    v.sps.push_back(std::move(this->packet(l)));
  }
  v.pps_count = byte();
  for (uint8_t i = 0; i < v.pps_count; ++i){
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
  auto l = ui32();
  return packet(l);
}
