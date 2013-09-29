#pragma once
#include <vector>
/*struct keyframe{
    int64_t file_position;
    int64_t timestamp;  //milliseconds? nano?
};
*/
struct keyframes {
  std::vector<uint64_t> positions;
  std::vector<uint64_t> times;
  void push_fileposition(uint64_t pos){
    positions.push_back(pos);
  }
  void push_time(double milli){
    times.push_back(uint64_t(milli * 10000));
  }
};
