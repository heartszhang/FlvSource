#pragma once
#include <cstdint>
#pragma pack(push,1)
struct raw_flv_header {
  uint8_t signature[3];
  uint8_t version;
  uint8_t has_video : 1;
  uint8_t: 1;
  uint8_t has_audio : 1;
  uint8_t: 5;
  uint32_t data_offset_be;
};
struct raw_flv_tag_header {
  uint8_t tag_type : 5;
  uint8_t filter : 1;
  uint8_t:2;
  uint8_t data_size[3];
  uint8_t timestamp[3];
  uint8_t timestamp_extended;
  uint8_t stream_id[3];
};
struct raw_audio_tag_header {
  uint8_t sound_type : 1;
  uint8_t sound_size : 1;
  uint8_t sound_rate : 2;
  uint8_t sound_format : 4;
};
struct raw_aac_packet_type {
  uint8_t aac_packet_type;
};
struct raw_video_tag_header {
  uint8_t codec_id : 4;
  uint8_t frame_type : 4;
};
struct raw_avc_packet_type {
  uint8_t avc_packet_type;
  uint8_t composition_time[3];
};

#pragma pack(pop)