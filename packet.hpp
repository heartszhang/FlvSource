#pragma once
#include <cstdint>
#include <cstring>
#include <utility>
struct packet{
  uint8_t *_      = nullptr;
  uint32_t length = 0;
  packet()        = default;
  packet(packet const&rhs) :packet(){
    if (rhs.length){
      length = rhs.length;
      _ = new uint8_t[length];
      memcpy_s(_, length, rhs._, rhs.length);
    }
  }
  packet(packet &&rhs) :packet(){
    std::swap(length, rhs.length);
    std::swap(_, rhs._);
  }
  packet&operator=(packet&&rhs){
    std::swap(length, rhs.length);
    std::swap(_, rhs._);
    return *this;
  }
  packet&operator=(packet const&rhs){
    if (rhs.length){
      length = rhs.length;
      _ = new uint8_t[length];
    }
    if(rhs._)  memcpy_s(_, length, rhs._, rhs.length);
    return *this;
  }
  packet(const uint8_t*d, uint32_t len) : length(len){
    if (length){
      _ = new uint8_t[length];
    }
    if(d)
      memcpy_s(_, length, d, len);
  }
  explicit packet(uint32_t len) : packet(nullptr, len){  }
  ~packet(){
    delete[] _;
  }
};
