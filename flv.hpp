#pragma once
#include <cstdint>
#include <memory>
namespace flv{
  enum class tag_type : int32_t{
    eof = 0,
      audio = 8,
      video = 9,
      script_data = 18,
  };
/*
  Format of SoundData. The following values are defined:
*/
enum class audio_codec : int32_t {
  lpcm = 0         , //  0 = Linear PCM, platform endian
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
    mp38k = 14     ,//  14= MP3 8 kHz
    ds = 15//  15= Device-specific sound
};

/*
Codec Identifier. The following values are defined:
*/
enum class video_codec : int32_t {
  sorenson        = 2,//2 = Sorenson H.263
    screen_video  = 3,//3 = Screen video
    on2vp6        = 4,//4 = On2 VP6
    on2vp6a       = 5,//5 = On2 VP6 with alpha channel
    screen_video2 = 6,//6 = Screen video version 2
    avc           = 7,//7 = AVC
};

/*
Type of video frame. The following values are defined:
*/
enum class frame_type : int8_t{
  key_frame             = 1 ,//= key frame (for AVC, a seekable frame)
    inter_frame         = 2 ,//= inter frame (for AVC, a non-seekable frame)
    disposable_inter_frame        = 3 ,//= disposable inter frame (H.263 only)
    generated_key_frame = 4 ,//= generated key frame (reserved for server use only)
    command_frame       = 5 //= video info/command frame
};

enum class video_frame_type : int8_t {
  key_frame = 1,
    inter_frame = 2,
    disposable_inter_frame = 3,
    generated_key_frame = 4,
    comamnd_frame = 5,
};
/*
The following values are defined:
0 = AVC sequence header
1 = AVC NALU
2 = AVC end of sequence (lower level NALU sequence ender is
not required or supported)
*/
enum class avc_packet_type : int8_t{
  avc_sequence_header = 0,
    avc_nalu = 1,
    avc_end_of_seq = 2
};
enum class sound_size : int8_t{
  _8bits = 0,
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
    stereo = 2
};
enum class aac_packet_type : int8_t {
  aac_sequence_header = 0,
    aac_raw = 1,
    aac_end_of_seq = 2
};
const static int flv_file_header_length        = 9;
const static int flv_tag_header_length = 11;
const static int flv_previous_tag_size_field_length = 4;
const static int flv_audio_header_length = 1;
const static int flv_video_header_length = 1;
const static int flv_aac_packet_type_length = 1;
const static int flv_avc_packet_type_length = 4;

}

struct audio_stream_header;
struct video_stream_header;

struct bit_reader{
  explicit bit_reader(const uint8_t*data, uint32_t length);
  uint32_t bits(uint8_t);
};

//AvcSpecificConfig
struct aac_profile {
  uint8_t const*data;
  uint32_t config_bytes;
  int32_t parse(){
    bit_reader reader(data, config_bytes);
    reader.bits(5);  // audioObjectType
    auto sample_rate = reader.bits(4);
    if (sample_rate = 0x0f){
      sample_rate = reader.bits(24);
    }
    auto channels = reader.bits(4);
    return 0;
  }
};


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
//avc_decoder_configuration_record
struct avc_decoder_configuratin_record;

enum class script_data_value_type : uint8_t {
  number = 0,
    boolean,
    string,
    object,
    movie_clip,
    null,
    undefined,
    reference,
    ecma = 8,
    object_end_marker = 9,
    array = 10,
    date,
    long_string
};

/*
Sampling rate. The following values are defined:
0 = 5.5 kHz
1 = 11 kHz
2 = 22 kHz
3 = 44 kHz
*/
/*
Size of each audio sample. This parameter only pertains to
uncompressed formats. Compressed formats always decode
to 16 bits internally.
0 = 8-bit samples
1 = 16-bit samples
*/
/*
Mono or stereo sound
0 = Mono sound
1 = Stereo sound
*/
#pragma pack(push,1)
struct raw_flv_header {
  uint8_t signature[3];
  uint8_t version;
  uint8_t:5;
  uint8_t has_audio : 1;
  uint8_t:1;
  uint8_t has_video : 1;
  uint32_t data_offset_be;
};
struct raw_flv_tag_header {
uint8_t:2;
uint8_t filter:1;
uint8_t tag_type : 5;
uint8_t data_size[3];
uint8_t timestamp[3];
uint8_t timestamp_extended;
uint8_t stream_id[3];
};
struct raw_audio_tag_header {
  uint8_t sound_format : 4;
  uint8_t sound_rate : 2;
  uint8_t sound_size : 1;
  uint8_t sound_type : 1;
};
struct raw_aac_packet_type {
  uint8_t aac_packet_type;
};
struct raw_video_tag_header {
  uint8_t frame_type : 4;
  uint8_t codec_id : 4;
};
struct raw_avc_packet_type {
  uint8_t avc_packet_type;
  uint8_t composition_time[3];
};

#pragma pack(pop)