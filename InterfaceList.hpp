#pragma once
#include <list>

template<typename Interface>
struct InterfaceList : protected std::list<Interface*>{
  typedef std::list<Interface*> supper;
  typedef Interface* Ptr;
  void push_back(Interface* i){
    if (i)
      i->AddRef();
    supper::push_back(i);
  }
  Interface* pop_front(){
    Interface*v = nullptr;
    if (!empty()){
      v = supper::front();
      supper::pop_front();
    }
    return v;  // needn't addref and release
  }
  bool empty()const{
    return supper::empty();
  }
  void clear(){
    for (auto i = supper::begin(); i != supper::end(); ++i){
      auto it = *i;
      if (it)
        it->Release();
    }
    supper::clear();
  }
  size_t size()const{
    return supper::size();
  }
};

