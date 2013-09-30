#pragma once
#include "keyframe.hpp"
#include "flv.hpp"
#include <vector>
struct flv_meta {
  // from onMetaData
  flv::audio_codec     audiocodecid;
  flv::video_codec     videocodecid;
  int32_t               audiodatarate = 0;  //kbits per second
  int32_t               audiodelay = 0;// seconds
  uint16_t              audiosamplesize = 0;  // 8bits /16bits
  uint64_t              audiosize = 0;
  uint64_t              duration; // seconds
  uint64_t              filesize; // total file size bytes
  uint64_t              datasize;
  uint32_t              height ;
  uint32_t              width;
  uint32_t              videodatarate = 0;  // bits per second
  int8_t                can_seek_to_end = 0;
  int8_t                stereo;
  uint32_t              audiosamplerate = 0;//bits per second
  uint32_t              framerate = 0;// frames per second
  uint32_t              last_timestamp = 0;
  uint32_t              last_keyframe_timestamp = 0;
  uint8_t               has_video = 0;
  uint8_t               has_audio = 0;
  uint8_t               has_metadata = 0;

  ::keyframes keyframes;
//  flv_meta() = default;
};
