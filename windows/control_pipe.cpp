#include "control_pipe.hpp"
#include "ow/control.hpp"
#include <sddl.h>
#include <bcrypt.h>
#include <array>
namespace ow::win {
namespace {
std::string token_sid(HANDLE token){DWORD bytes{};GetTokenInformation(token,TokenUser,nullptr,0,&bytes);std::vector<unsigned char> buffer(bytes);require(GetTokenInformation(token,TokenUser,buffer.data(),bytes,&bytes),"Read control owner");LPWSTR sid=nullptr;require(ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid,&sid),"Format control owner");std::wstring out(sid);LocalFree(sid);return utf8(out);}
bool completed(HANDLE pipe,OVERLAPPED& ov,DWORD& bytes,const std::atomic_bool* stop,Tick deadline){
 for(;;){if((stop&&*stop)||monotonic_ms()>=deadline){CancelIoEx(pipe,&ov);GetOverlappedResult(pipe,&ov,&bytes,TRUE);return false;}auto wait=WaitForSingleObject(ov.hEvent,50);if(wait==WAIT_OBJECT_0)return GetOverlappedResult(pipe,&ov,&bytes,FALSE)!=FALSE;if(wait!=WAIT_TIMEOUT)return false;}
}
std::string exchange(HANDLE pipe,const std::string* output,const std::atomic_bool* stop){
 Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));if(!event)fail("Create control event");OVERLAPPED ov{};ov.hEvent=event.get();DWORD bytes{};std::vector<char> buffer(output?0:control_frame_limit);
 BOOL ok=output?WriteFile(pipe,output->data(),static_cast<DWORD>(output->size()),&bytes,&ov):ReadFile(pipe,buffer.data(),static_cast<DWORD>(buffer.size()),&bytes,&ov);
 if(!ok&&(GetLastError()!=ERROR_IO_PENDING||!completed(pipe,ov,bytes,stop,monotonic_ms()+2000)))throw std::runtime_error("Control unavailable or timed out");
 if(output){if(bytes!=output->size())throw std::runtime_error("Short control write");return {};}
 if(!bytes||bytes>=buffer.size())throw std::runtime_error("Invalid control frame");return {buffer.data(),bytes};
}
bool owner_client(HANDLE pipe,const std::string& owner){
 if(!ImpersonateNamedPipeClient(pipe))return false;
 Handle token;HANDLE raw{};const bool opened=OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,TRUE,&raw)!=FALSE;token.reset(raw);bool accepted=false;
 try{if(opened)accepted=token_sid(token.get())==owner;}catch(...){accepted=false;}
 if(!RevertToSelf())std::terminate(); // Never execute handlers while impersonating.
 return accepted;
}
}
std::string process_owner_sid(HANDLE process){HANDLE token{};require(OpenProcessToken(process,TOKEN_QUERY,&token),"Open control process token");Handle owned(token);return token_sid(token);}
std::wstring control_pipe_name(const std::filesystem::path& root){
 // Root-derived name isolates native fixtures. No supplied root ever travels over IPC.
 auto name=std::filesystem::absolute(root).lexically_normal().wstring();while(name.size()>3&&(name.back()==L'\\'||name.back()==L'/'))name.pop_back();for(auto& c:name)c=static_cast<wchar_t>(towlower(c));
 BCRYPT_ALG_HANDLE alg{};if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("Control namespace hash failed");std::array<unsigned char,32> hash{};
 const auto result=BCryptHash(alg,nullptr,0,reinterpret_cast<PUCHAR>(name.data()),static_cast<ULONG>(name.size()*sizeof(wchar_t)),hash.data(),static_cast<ULONG>(hash.size()));BCryptCloseAlgorithmProvider(alg,0);if(result<0)throw std::runtime_error("Control namespace hash failed");
 std::wstring hex;for(auto b:hash){hex+=L"0123456789abcdef"[b>>4];hex+=L"0123456789abcdef"[b&15];}
 return L"\\\\.\\pipe\\OrganizerWorker.Control.v1."+wide(process_owner_sid(GetCurrentProcess()))+L"."+hex;
}
ControlPipe::ControlPipe(const std::filesystem::path& root,std::function<json::object(const json::object&)> handler){
 const auto owner=process_owner_sid(GetCurrentProcess());const auto sddl=L"D:P(A;;GA;;;"+wide(owner)+L")";PSECURITY_DESCRIPTOR sd{};require(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&sd,nullptr),"Build owner-only pipe ACL");SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};
 pipe_.reset(CreateNamedPipeW(control_pipe_name(root).c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,static_cast<DWORD>(control_frame_limit),static_cast<DWORD>(control_frame_limit),2000,&sa));LocalFree(sd);if(!pipe_)fail("Create owner control pipe");
 thread_=std::jthread([this,owner,handler=std::move(handler)]{
  while(!stop_){
   Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));if(!event)return;OVERLAPPED ov{};ov.hEvent=event.get();DWORD bytes{};bool connected=ConnectNamedPipe(pipe_.get(),&ov)!=FALSE;
   if(!connected){const auto error=GetLastError();connected=error==ERROR_PIPE_CONNECTED||(error==ERROR_IO_PENDING&&completed(pipe_.get(),ov,bytes,&stop_,monotonic_ms()+1000));}
   if(!connected){DisconnectNamedPipe(pipe_.get());continue;}
   try{
    auto input=exchange(pipe_.get(),nullptr,nullptr);json::object response;
    if(!owner_client(pipe_.get(),owner))response={{"schema",1},{"ok",false},{"exit_code",4},{"error","Control owner denied"}};
    else try{auto request=parse(input,control_frame_limit-1).as_object();if(number(request,"schema",1)!=1)throw std::runtime_error("Unsupported control schema");response=handler(validate_control_request(request));}
    catch(const ControlFailure& e){response={{"schema",1},{"ok",false},{"exit_code",e.exit_code},{"error","Control persistence or access failure; read back before retrying"}};}
    catch(const std::exception&){response={{"schema",1},{"ok",false},{"exit_code",2},{"error","Command rejected; check arguments, configuration and worker log"}};}
    auto output=json::serialize(response);if(output.size()>=control_frame_limit)output=json::serialize(json::object{{"schema",1},{"ok",false},{"exit_code",5},{"error","Control response exceeds limit"}});exchange(pipe_.get(),&output,nullptr);
    // Read receipt prevents DisconnectNamedPipe from discarding an unread reply.
    const auto receipt=exchange(pipe_.get(),nullptr,nullptr);if(receipt!="read")throw std::runtime_error("Invalid control receipt");
   }catch(...){/* Broken/oversized/idle clients cannot stop the controller. */}
   DisconnectNamedPipe(pipe_.get());
  }
 });
}
ControlPipe::~ControlPipe(){stop_=true;if(thread_.joinable())thread_.join();}
json::object control_call(const std::filesystem::path& root,const json::object& request){
 const auto name=control_pipe_name(root);if(!WaitNamedPipeW(name.c_str(),2000)){auto error=GetLastError();if(error==ERROR_FILE_NOT_FOUND||error==ERROR_SEM_TIMEOUT||error==ERROR_PIPE_BUSY)throw ControlUnavailable("Control absent or busy before connection");fail("Inspect owner control availability",error);}
 Handle pipe(CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,nullptr));if(!pipe){auto error=GetLastError();if(error==ERROR_FILE_NOT_FOUND||error==ERROR_PIPE_BUSY)throw ControlUnavailable("Control unavailable before connection");fail("Open owner control pipe",error);}
 ULONG pid{};require(GetNamedPipeServerProcessId(pipe.get(),&pid),"Identify control server");Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));if(!process||process_owner_sid(process.get())!=process_owner_sid(GetCurrentProcess()))throw std::runtime_error("Control server owner denied");
 DWORD mode=PIPE_READMODE_MESSAGE;require(SetNamedPipeHandleState(pipe.get(),&mode,nullptr,nullptr),"Set control framing");auto output=json::serialize(request);if(output.size()>=control_frame_limit)throw std::runtime_error("Control request exceeds limit");exchange(pipe.get(),&output,nullptr);auto response=parse(exchange(pipe.get(),nullptr,nullptr),control_frame_limit-1).as_object();const std::string receipt="read";exchange(pipe.get(),&receipt,nullptr);if(number(response,"schema",1)!=1||!response.at("ok").is_bool()||(number(response,"exit_code",5)==0)!=response.at("ok").as_bool())throw std::runtime_error("Invalid control response schema");return response;
}
}
