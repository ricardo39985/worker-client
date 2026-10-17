#pragma once
#include "ow/progress.hpp"
#include <array>
#include <filesystem>
#include <fstream>
namespace ow {
// One reader per child invocation. A CPU retry gets a fresh parser and cursor;
// its caller begins a new stage generation before launching the child.
class EncoderProgressFile {
 std::filesystem::path path_;
 EncoderProgress encoder_;
 std::uint64_t position_{};
public:
 explicit EncoderProgressFile(std::filesystem::path path):path_(std::move(path)){}
 void update(ProgressCounter& progress,std::int64_t now){
  std::ifstream log(path_,std::ios::binary);if(!log)return;
  log.seekg(0,std::ios::end);auto end=log.tellg();if(end<0)return;
  auto length=static_cast<std::uint64_t>(end);if(length<position_)position_=0;
  log.seekg(static_cast<std::streamoff>(position_));std::array<char,8192> buffer{};
  while(log){
   log.read(buffer.data(),buffer.size());auto count=log.gcount();
   if(count>0){encoder_.consume(std::string_view(buffer.data(),static_cast<std::size_t>(count)));position_+=static_cast<std::uint64_t>(count);}
  }
  if(encoder_.duration_us())progress.set_total(encoder_.duration_us(),now);
  progress.update(encoder_.position_us(),now);
 }
};
}
