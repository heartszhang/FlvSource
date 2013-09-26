#pragma once

namespace bigendian{
  uint32_t touint32(uint8_t*data, uint8_t const*, uint8_t**pat);
  uint32_t touint24(uint8_t*data, uint8_t const*, uint8_t**pat);
  uint16_t touint16(uint8_t*data, uint8_t const*, uint8_t**pat);
  uint8_t touint8(uint8_t*data, uint8_t const*, uint8_t**pat);
  int32_t toint32(uint8_t*data, uint8_t const*, uint8_t**pat);
//  int32_t toint24(uint8_t*data, size_t);
  int16_t toint16(uint8_t*data, uint8_t const*, uint8_t**pat);
  int8_t toint8(uint8_t*data, uint8_t const*, uint8_t**pat);
}
