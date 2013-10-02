#pragma once
#include <cstdint>
#include <cassert>
#include <set>
#include <vector>
#include <algorithm>

struct keyframes {  // todo: implement rvalue copy ctr
  std::vector<uint64_t> positions;
  std::vector<uint64_t> times;

  void push_fileposition(uint64_t pos){
    positions.push_back(pos);  // ignore return value
  }
  void push_time(double milli){
    auto nano = uint64_t(milli * 10000);
    times.push_back(nano); // milli to nano
//    time_index.insert(nano);
  }
  uint64_t seek(uint64_t nano){
    assert(positions.size() == times.size());
    return binary_search(nano, 0, int32_t(times.size()) - 1);
  }
  uint64_t binary_search(uint64_t v, int32_t start, int32_t tail){
    if (start >= tail){
      return positions[start];
    }
    auto m = (start + tail) /2;
    if (times[m] == v)
      return positions[m];
    if (times[m] < v)
      return binary_search(v, m +1, tail);
    else
      return binary_search(v, start, m-1);
  }
};
