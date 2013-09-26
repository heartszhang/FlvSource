#pragma once
#include "keyframe.hpp"
#include "flv.hpp"
#include <vector>
struct flv_meta {
  // from onMetaData
  flv::audio_codec     audiocodecid;
  int32_t     audiodatarate;  //kbits per second
  int32_t     audiodelay;// seconds
  uint16_t     audiosamplesize;  // 8bits /16bits
  int32_t     duration; // seconds
  int64_t     filesize; // total file size bytes
  uint32_t     height;
  uint32_t     width;
  flv::video_codec     videocodecid;
  uint32_t     videodatarate;  // kbits per second
  int8_t      can_seek_to_end;
  int8_t      stereo;
  uint32_t      audiosamplerate;//41k/22k?
  uint32_t      framerate;// frames per second

  std::vector<keyframe> keyframes;
};
