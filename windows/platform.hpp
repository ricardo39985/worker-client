#include <chrono>
#include <memory>
#include <stdexcept>
#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include <functional>
#include <vector>
#include <mutex>
#include <atomic>
#include "ow/json.hpp"
#include "ow/admission.hpp"
namespace ow::win {
class Handle {
 HANDLE value_{};
public:
 explicit Handle(HANDLE value=nullptr):value_(value){}
 ~Handle(){reset();}Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
 Handle(Handle&& other) noexcept:value_(other.release()){}
 Handle& operator=(Handle&& other) noexcept{if(this!=&other){reset();value_=other.release();}return *this;}
 HANDLE get() const{return value_;}explicit operator bool() const{return value_&&value_!=INVALID_HANDLE_VALUE;}
 HANDLE release(){auto v=value_;value_=nullptr;return v;}
 void reset(HANDLE v=nullptr){if(*this)CloseHandle(value_);value_=v;}
};
[[noreturn]] void fail(const char* operation,DWORD code=GetLastError());
inline void require(BOOL ok,const char* operation){if(!ok)fail(operation);}
std::wstring wide(const std::string&);
std::string utf8(const std::wstring&);
Tick monotonic_ms();std::int64_t utc_ms();
std::filesystem::path data_directory();
std::filesystem::path executable_directory();
std::string read_file(const std::filesystem::path&,std::size_t limit=256*1024);
void atomic_write(const std::filesystem::path&,const std::string&);
std::string random_hex(std::size_t bytes);
void publish_runtime_ready(const std::filesystem::path&);
void clear_runtime_ready(const std::filesystem::path&) noexcept;
std::string sha256_file(const std::filesystem::path&,const std::function<void(std::uint64_t)>& progress={});
std::string protect(const std::string&);std::string unprotect(const std::string&);
std::string public_pairing_key(const std::filesystem::path& private_file);
std::string sign_pairing_challenge(const std::filesystem::path& private_file,const std::string& challenge);
Resources available_resources(const std::filesystem::path&,std::uint64_t reserve_mb);
bool elevated();
class Log {
 std::filesystem::path path_;std::mutex mutex_;Handle file_;
 void open();
public:
 explicit Log(std::filesystem::path p):path_(std::move(p)){open();}
 void write(const std::string& message);
};
}
