#pragma once
#include "platform.hpp"
#include <winhttp.h>
#include <stop_token>
#include <functional>
#include <memory>
#include <optional>
namespace ow::win {
struct Response {DWORD status{};std::string body;};
class Http {
public:
 Response request(const std::string& method,const std::string& url,const std::string& body={},const std::string& bearer={});
 void download(const std::string& url,const std::filesystem::path& target,std::uint64_t expected,const std::function<bool()>& cancelled);
 void upload(const std::string& url,const std::filesystem::path& source,const std::string& content_type,const std::function<bool()>& cancelled);
};
class WebSocket {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 WebSocket(const std::string& https_url,const std::string& bearer);
 ~WebSocket();WebSocket(const WebSocket&)=delete;WebSocket& operator=(const WebSocket&)=delete;
 void send(const std::string& message);
 std::optional<std::string> receive(unsigned wait_ms=100);
};
}
