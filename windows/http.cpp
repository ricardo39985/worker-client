#include "http.hpp"
#include <array>
#include <fstream>
#include <condition_variable>
#include <stdexcept>
namespace ow::win {
namespace {
class Internet {
 HINTERNET value_{};
public:
 explicit Internet(HINTERNET v=nullptr):value_(v){if(!v)fail("Create HTTP handle");}
 ~Internet(){if(value_)WinHttpCloseHandle(value_);}Internet(const Internet&)=delete;
 HINTERNET get() const{return value_;}
};
struct URL {
 std::wstring host,path;INTERNET_PORT port{};
 explicit URL(const std::string& url){
 auto value=wide(url);URL_COMPONENTS c{};c.dwStructSize=sizeof(c);c.dwHostNameLength=c.dwUrlPathLength=c.dwExtraInfoLength=c.dwUserNameLength=c.dwPasswordLength=static_cast<DWORD>(-1);
 require(WinHttpCrackUrl(value.c_str(),static_cast<DWORD>(value.size()),0,&c),"Parse HTTPS endpoint");
 if(c.nScheme!=INTERNET_SCHEME_HTTPS||c.dwUserNameLength||c.dwPasswordLength||c.nPort!=443||url.find_first_of("\r\n#")!=std::string::npos)throw std::runtime_error("Only credential-free HTTPS on port 443 is permitted");
 host.assign(c.lpszHostName,c.dwHostNameLength);path.assign(c.lpszUrlPath,c.dwUrlPathLength);path.append(c.lpszExtraInfo,c.dwExtraInfoLength);if(path.empty())path=L"/";port=c.nPort;
 }
};
void options(HINTERNET request){
 DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;require(WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects)),"Disable HTTP redirects");
 DWORD logon=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;require(WinHttpSetOption(request,WINHTTP_OPTION_AUTOLOGON_POLICY,&logon,sizeof(logon)),"Disable ambient Windows authentication");
}
std::wstring headers(const std::string& bearer,const wchar_t* content=L"application/json"){
 if(bearer.find_first_of("\r\n\0",0,3)!=std::string::npos)throw std::runtime_error("Invalid bearer credential");
 std::wstring h=L"Content-Type: ";h+=content;h+=L"\r\n";if(!bearer.empty())h+=L"Authorization: Bearer "+wide(bearer)+L"\r\n";return h;
}
DWORD status(HINTERNET r){DWORD value{},size=sizeof(value);require(WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&value,&size,WINHTTP_NO_HEADER_INDEX),"Read HTTP status");return value;}
struct Request {
 URL url;Internet session,connection,request;
 Request(const std::string& method,const std::string& address):url(address),
  session(WinHttpOpen(L"OrganizerWorker/0.1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)),
  connection(WinHttpConnect(session.get(),url.host.c_str(),url.port,0)),
  request(WinHttpOpenRequest(connection.get(),wide(method).c_str(),url.path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)){
  require(WinHttpSetTimeouts(request.get(),5000,5000,10000,15000),"Set HTTPS timeouts");options(request.get());}
 void send(const std::wstring& h,void* data,DWORD length,DWORD total){require(WinHttpSendRequest(request.get(),h.c_str(),static_cast<DWORD>(h.size()),data,length,total,0),"Send HTTPS request");}
 void response(){require(WinHttpReceiveResponse(request.get(),nullptr),"Receive HTTPS response");}
};
void check_cancel(const std::function<bool()>& cancelled){if(cancelled())throw std::runtime_error("job cancelled or lease expired");}
}
Response Http::request(const std::string& method,const std::string& url,const std::string& body,const std::string& bearer){
 if(body.size()>256*1024)throw std::runtime_error("Control request too large");Request r(method,url);r.send(headers(bearer),body.empty()?WINHTTP_NO_REQUEST_DATA:const_cast<char*>(body.data()),static_cast<DWORD>(body.size()),static_cast<DWORD>(body.size()));r.response();Response out{status(r.request.get()),{}};
 std::array<char,16384> b{};for(;;){DWORD n=0;require(WinHttpReadData(r.request.get(),b.data(),static_cast<DWORD>(b.size()),&n),"Read HTTPS body");if(!n)break;if(out.body.size()+n>256*1024)throw std::runtime_error("Control response too large");out.body.append(b.data(),n);}return out;
}
void Http::download(const std::string& url,const std::filesystem::path& target,std::uint64_t expected,const std::function<bool()>& cancelled){
 check_cancel(cancelled);Request r("GET",url);r.send(L"Accept-Encoding: identity\r\n",WINHTTP_NO_REQUEST_DATA,0,0);r.response();if(status(r.request.get())!=200)throw std::runtime_error("Input download HTTP status was not 200");
 std::ofstream file(target,std::ios::binary|std::ios::trunc);if(!file)throw std::runtime_error("Cannot create job input");std::array<char,64*1024> buffer{};std::uint64_t total=0;
 for(;;){check_cancel(cancelled);DWORD n=0;require(WinHttpReadData(r.request.get(),buffer.data(),static_cast<DWORD>(buffer.size()),&n),"Download input");if(!n)break;if(n>expected-total)throw std::runtime_error("Input exceeds approved byte budget");file.write(buffer.data(),n);if(!file)throw std::runtime_error("Input write failed (disk full?)");total+=n;}
 file.close();if(total!=expected)throw std::runtime_error("Input byte count differs from job manifest");
}
void Http::upload(const std::string& url,const std::filesystem::path& source,const std::string& mime,const std::function<bool()>& cancelled){
 check_cancel(cancelled);auto length=std::filesystem::file_size(source);if(length>1024ull*1024*1024)throw std::runtime_error("Output exceeds transport limit");Request r("PUT",url);r.send(headers({},wide(mime).c_str()),WINHTTP_NO_REQUEST_DATA,0,static_cast<DWORD>(length));std::ifstream file(source,std::ios::binary);if(!file)throw std::runtime_error("Cannot read output");std::array<char,64*1024> b{};std::uint64_t total=0;
 while(file){check_cancel(cancelled);file.read(b.data(),b.size());auto n=file.gcount();if(!n)break;DWORD sent=0;require(WinHttpWriteData(r.request.get(),b.data(),static_cast<DWORD>(n),&sent),"Upload output");if(sent!=n)throw std::runtime_error("Short output upload");total+=sent;}
 if(!file.eof()||total!=length)throw std::runtime_error("Output changed or could not be read");r.response();auto code=status(r.request.get());if(code<200||code>=300)throw std::runtime_error("Output upload rejected");check_cancel(cancelled);
}
// Asynchronous WinHTTP completions let the receive remain pending while the
// dispatcher sends heartbeats. No public listener or busy-wait receive loop.
struct WebSocket::Impl {
 struct Context {
  std::mutex mutex;std::condition_variable cv;bool sent{},headers{},read{},written{},closed{};DWORD error{},bytes{};WINHTTP_WEB_SOCKET_BUFFER_TYPE kind{};
 } request_context,socket_context;
 URL url;Internet session,connection;HINTERNET request{},socket{};std::array<char,16384> buffer{};bool read_pending{},request_callback_registered{};std::string assembled,send_buffer;
 static void CALLBACK callback(HINTERNET,DWORD_PTR raw,DWORD event,LPVOID info,DWORD){
  if(!raw)return;auto& c=*reinterpret_cast<Context*>(raw);std::lock_guard lock(c.mutex);
  if(event==WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE)c.sent=true;
  else if(event==WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE)c.headers=true;
  else if(event==WINHTTP_CALLBACK_STATUS_REQUEST_ERROR)c.error=static_cast<WINHTTP_ASYNC_RESULT*>(info)->dwError;
  else if(event==WINHTTP_CALLBACK_STATUS_READ_COMPLETE){auto* s=static_cast<WINHTTP_WEB_SOCKET_STATUS*>(info);c.bytes=s->dwBytesTransferred;c.kind=s->eBufferType;c.read=true;}
  else if(event==WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE)c.written=true;
  else if(event==WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING)c.closed=true;
  c.cv.notify_all();
 }
 static void wait(Context& c,bool Context::*flag){std::unique_lock lock(c.mutex);if(!c.cv.wait_for(lock,std::chrono::seconds(15),[&]{return c.*flag||c.error||c.closed;}))throw std::runtime_error("WebSocket operation timed out");if(c.error||c.closed)throw std::runtime_error("WebSocket disconnected");}
 static void close(HINTERNET& h,Context& c,bool registered=true) noexcept {
  if(!h)return;auto v=h;h=nullptr;
  // Context and buffers cannot be freed before the final asynchronous callback.
  if(!WinHttpCloseHandle(v))std::terminate();
  if(registered){std::unique_lock lock(c.mutex);c.cv.wait(lock,[&]{return c.closed;});}
 }
 explicit Impl(const std::string& address,const std::string& bearer):url(address),session(WinHttpOpen(L"OrganizerWorker/0.1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,WINHTTP_FLAG_ASYNC)),connection(WinHttpConnect(session.get(),url.host.c_str(),url.port,0)){
  try{
  request=WinHttpOpenRequest(connection.get(),L"GET",url.path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);if(!request)fail("Create worker connection");
  DWORD_PTR context=reinterpret_cast<DWORD_PTR>(&request_context);require(WinHttpSetOption(request,WINHTTP_OPTION_CONTEXT_VALUE,&context,sizeof(context)),"Set connection context");
  auto previous=WinHttpSetStatusCallback(request,callback,WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS|WINHTTP_CALLBACK_FLAG_HANDLES,0);if(previous==WINHTTP_INVALID_STATUS_CALLBACK){WinHttpCloseHandle(request);request=nullptr;fail("Register network callbacks");}
  request_callback_registered=true;
  options(request);require(WinHttpSetTimeouts(request,5000,5000,10000,15000),"Set worker connection timeouts");require(WinHttpSetOption(request,WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,nullptr,0),"Request WebSocket upgrade");auto h=headers(bearer);
  BOOL ok=WinHttpSendRequest(request,h.c_str(),static_cast<DWORD>(h.size()),WINHTTP_NO_REQUEST_DATA,0,0,context);if(!ok&&GetLastError()!=ERROR_IO_PENDING)fail("Begin worker connection");wait(request_context,&Context::sent);
  ok=WinHttpReceiveResponse(request,nullptr);if(!ok&&GetLastError()!=ERROR_IO_PENDING)fail("Receive upgrade response");wait(request_context,&Context::headers);
  auto code=status(request);if(code!=101)throw std::runtime_error("Worker upgrade rejected, HTTP "+std::to_string(code));
  socket=WinHttpWebSocketCompleteUpgrade(request,reinterpret_cast<DWORD_PTR>(&socket_context));if(!socket)fail("Complete worker connection");
  close(request,request_context,request_callback_registered);
  }catch(...){close(socket,socket_context);close(request,request_context,request_callback_registered);throw;}
 }
 ~Impl(){close(socket,socket_context);close(request,request_context,request_callback_registered);}
};
WebSocket::WebSocket(const std::string& url,const std::string& token):impl_(std::make_unique<Impl>(url,token)){}
WebSocket::~WebSocket()=default;
void WebSocket::send(const std::string& message){if(message.size()>256*1024)throw std::runtime_error("Outgoing control message too large");auto& i=*impl_;{std::lock_guard lock(i.socket_context.mutex);i.socket_context.written=false;}
 i.send_buffer=message;
 auto e=WinHttpWebSocketSend(i.socket,WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,i.send_buffer.data(),static_cast<DWORD>(i.send_buffer.size()));if(e!=NO_ERROR&&e!=ERROR_IO_PENDING)fail("Send worker message",e);Impl::wait(i.socket_context,&Impl::Context::written);
}
std::optional<std::string> WebSocket::receive(unsigned wait_ms){
 auto& i=*impl_;auto& c=i.socket_context;if(!i.read_pending){{std::lock_guard lock(c.mutex);c.read=false;}i.read_pending=true;auto e=WinHttpWebSocketReceive(i.socket,i.buffer.data(),static_cast<DWORD>(i.buffer.size()),nullptr,nullptr);if(e!=NO_ERROR&&e!=ERROR_IO_PENDING)fail("Receive worker message",e);}
 std::unique_lock lock(c.mutex);if(!c.cv.wait_for(lock,std::chrono::milliseconds(wait_ms),[&]{return c.read||c.error||c.closed;}))return {};if(c.error||c.closed)throw std::runtime_error("Worker connection lost");i.read_pending=false;
 if(c.kind==WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)throw std::runtime_error("Coordinator closed connection");if(c.kind!=WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE&&c.kind!=WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE)throw std::runtime_error("Unexpected binary worker frame");
 if(i.assembled.size()+c.bytes>256*1024)throw std::runtime_error("Incoming control message too large");i.assembled.append(i.buffer.data(),c.bytes);
 if(c.kind==WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE){std::string out;out.swap(i.assembled);return out;}return {};
}
}
