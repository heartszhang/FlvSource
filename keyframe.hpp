#pragma once
#include <set>

struct keyframes {  // todo: implement rvalue copy ctr
  std::set<uint64_t> positions;
  std::set<uint64_t> times;  // nano seconds

  void push_fileposition(uint64_t pos){
    positions.insert(pos);  // ignore return value
  }
  void push_time(double milli){
    times.insert(uint64_t(milli * 10000)); // milli to nano
  }
};
