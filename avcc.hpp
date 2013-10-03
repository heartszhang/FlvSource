#pragma once
#include <vector>
#include "bigendian.hpp"
#include "packet.hpp"

namespace flv {
struct avcc //avc_decoder_configuration_record
{
  uint8_t profile   = 0;  //AVCProfileIndication
  uint8_t level     = 0;  //AVCLevelIndication
  uint8_t nal       = 0;  // <- lengthSizeMinusOne
  std::vector<packet> sps;
  std::vector<packet> pps;
  packet code_private_data()const;
  packet sequence_header()const;
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
/*
aligned(8) class AVCDecoderConfigurationRecord {
unsigned int(8) configurationVersion = 1;
unsigned int(8) AVCProfileIndication;
unsigned int(8) profile_compatibility;
unsigned int(8) AVCLevelIndication;
bit(6) reserved = '111111'b;
unsigned int(2) lengthSizeMinusOne;
bit(3) reserved = '111'b;
unsigned int(5) numOfSequenceParameterSets;
for (i=0; i< numOfSequenceParameterSets; i++) {
unsigned int(16) sequenceParameterSetLength ;
bit(8*sequenceParameterSetLength) sequenceParameterSetNALUnit;
}
unsigned int(8) numOfPictureParameterSets;
for (i=0; i< numOfPictureParameterSets; i++) {
unsigned int(16) pictureParameterSetLength;
bit(8*pictureParameterSetLength) pictureParameterSetNALUnit;
}
}
*/
