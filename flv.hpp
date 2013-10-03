#pragma once
#include <cstdint>
#include <memory>
#include "flv_raw_header.hpp"
namespace flv{
  enum class tag_type : int32_t{
    eof = 0,
      audio = 8,
      video = 9,
      script_data = 18,
  };

// audio codec id, defined by flv spec
enum class audio_codec : int32_t {
  lpcm = 0         , //  0 = Linear PCM, platform endian, MFAudioFormat_PCM?
    adpcm          , //  1  = ADPCM
    mp3            , //   2 = MP3
    lpcm_le        ,//  3= Linear PCM, little endian
    nellymoser_16k ,//  4= Nellymoser 16 kHz mono
    nellymoser_8k  ,//  5= Nellymoser 8 kHz mono
    nellymoser     ,//  6= Nellymoser
    g711a          ,//  7= G.711 A-law logarithmic PCM
    g711mu         ,//  8= G.711 mu-law logarithmic PCM
    aac = 10       ,//  10= AAC
    speex          ,//  11= Speex
    mp38k = 14     ,//  14= MP3 8 kHz, MFAudioFormat_MP3
    ds = 15//  15= Device-specific sound
};

//video codec id defined by flv spec
enum class video_codec : int32_t {
  sorenson        = 2,//2 = Sorenson H.263
    screen_video  = 3,//3 = Screen video
    on2vp6        = 4,//4 = On2 VP6
    on2vp6a       = 5,//5 = On2 VP6 with alpha channel
    screen_video2 = 6,//6 = Screen video version 2
    avc           = 7,//7 = AVC
};

// video frame type , defined by flv spec
enum class frame_type : int8_t{
  key_frame             = 1 ,//= key frame (for AVC, a seekable frame)
    inter_frame         = 2 ,//= inter frame (for AVC, a non-seekable frame)
    disposable_inter_frame        = 3 ,//= disposable inter frame (H.263 only)
    generated_key_frame = 4 ,//= generated key frame (reserved for server use only)
    command_frame       = 5 //= video info/command frame
};

enum class avc_packet_type : int8_t{
  avc_sequence_header = 0,              //AVC sequence header
    avc_nalu          = 1,              // AVC NALU
    avc_end_of_seq    = 2               //AVC end of sequence (lower level NALU sequence ender is
};
enum class sound_size : int8_t{
  _8bits  = 0,
  _16bits = 1,
};
enum class sound_rate : int8_t {
  _5k = 0,
    _11k = 1,
    _22k = 2,
    _44k = 3
};
enum class sound_type : int8_t {
  mono = 0,
    stereo = 1
};
enum class aac_packet_type : int8_t {
  aac_sequence_header = 0,
    aac_raw = 1,
    aac_end_of_seq = 2,
};
enum class script_data_value_type : uint8_t {
  number              = 0,
    boolean,
    string,
    object,
    movie_clip,
    null,
    undefined,
    reference,
    ecma              = 8,
    object_end_marker = 9,
    array             = 10,
    date,
    long_string
};

const static int     flv_file_header_length             = 9;
const static int     flv_tag_header_length              = 11;
const static int     flv_previous_tag_size_field_length = 4;
const static int     flv_audio_header_length            = 1;
const static int     flv_video_header_length            = 1;
const static int     flv_aac_packet_type_length         = 1;
const static int     flv_avc_packet_type_length         = 4;  //avc_packet_type + composite_time
const static uint8_t flv_file_header_audio_mask         = 1 << 2;
const static uint8_t flv_file_header_video_mask         = 1 << 0;
const static uint8_t flv_tag_header_type_mask           = 0x1f;
const static uint8_t flv_tag_header_filter_mask         = 5;  //bits
}

struct audio_packet_header;
struct video_packet_header;

