#pragma once
#include <cassert>
#include <vector>
#include <utility>
#include "bigendian.hpp"
#include "packet.hpp"
namespace flv {
struct avcc
{
  uint8_t profile   = 0;
  uint8_t level     = 0;
  uint8_t nal       = 0;
  uint8_t sps_count = 0;
  uint8_t pps_count = 0;
  std::vector<packet> sps;
  std::vector<packet> pps;
  packet code_private_data()const;
};
struct avcc_reader : public bigendian::binary_reader
{
  avcc_reader(uint8_t const*d, uint32_t len):binary_reader(d, len){}
  flv::avcc avcc();
};
struct nalu_reader : public bigendian::binary_reader{
  nalu_reader(uint8_t const*d, uint32_t len) : binary_reader(d, len){};
  ::packet nalu();
};
}