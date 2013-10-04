#pragma once
#include <cstdint>
#include <cassert>
#include <set>
#include <vector>
#include <algorithm>

struct keyframe {
  uint64_t position = 0;
  uint64_t time = 0;
  keyframe(uint64_t pos, uint64_t tm) : position(pos), time(tm) {};
  keyframe() = default;
};
struct keyframes {  // todo: implement rvalue copy ctr
  std::vector<uint64_t> positions;
  std::vector<uint64_t> times;

  void push_fileposition(uint64_t pos){
    positions.push_back(pos);  // ignore return value
  }
  void push_time(double sec){
    auto nano = uint64_t(sec * 10000ull * 1000ull);
    times.push_back(nano); // milli to nano
//    time_index.insert(nano);
  }
  keyframe seek(uint64_t nano){
    assert(positions.size() == times.size());
    return binary_search(nano, 0, int32_t(times.size()) - 1);
  }
  keyframe binary_search(uint64_t v, int32_t start, int32_t tail) {
    if (start >= tail) {
      return keyframe{ positions[start], times[start] };
    }
    auto m = (start + tail) / 2;
    if (times[m] == v)
      return keyframe{ positions[m], times[m] };
    if (times[m] < v)
      return binary_search(v, m +1, tail);
    else
      return binary_search(v, start, m-1);
  }
};
