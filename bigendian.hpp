#pragma once
#include <cstdint>

namespace bigendian{
uint32_t touint32(uint8_t const*data);
uint32_t touint24(uint8_t const* data);
uint16_t touint16(uint8_t const*data);
uint8_t touint8(uint8_t const*data);
int32_t toint32(uint8_t const*data);

int16_t toint16(uint8_t const*data);
int8_t toint8(uint8_t const*data);
  struct binary_reader{
    const uint8_t* data;
    uint32_t length;
    uint32_t pointer;
    explicit binary_reader(const uint8_t*dat, uint32_t length)
    :data(dat), length(length), pointer(0){

    }
    binary_reader() = delete;
    uint8_t byte();
    uint16_t ui16();
    uint32_t ui24();
    uint32_t ui32();
    uint64_t ui64();    
    double numberic();
    void skip(uint32_t bytes);
  };
}