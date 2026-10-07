#include "platform.hpp"
#include <shlobj.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <chrono>
#include <fstream>
#include <array>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <cstdio>
namespace ow::win {
[[noreturn]] void fail(const char* op,DWORD code){throw std::runtime_error(std::string(op)+" failed (Windows error "+std::to_string(code)+")");}
std::wstring wide(const std::string& s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);if(!n)fail("UTF-8 decode");std::wstring out(n,0);require(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),out.data(),n)>0,"UTF-8 decode");return out;}
std::string utf8(const std::wstring& s){if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);if(!n)fail("UTF-8 encode");std::string out(n,0);require(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),out.data(),n,nullptr,nullptr)>0,"UTF-8 encode");return out;}
Tick monotonic_ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
std::int64_t utc_ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
std::filesystem::path data_directory(){PWSTR p=nullptr;HRESULT hr=SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&p);if(FAILED(hr))fail("Locate LocalAppData",static_cast<DWORD>(hr));std::filesystem::path out=std::filesystem::path(p)/"OrganizerWorker";CoTaskMemFree(p);return out;}
std::filesystem::path executable_directory(){std::wstring p(32768,0);DWORD n=GetModuleFileNameW(nullptr,p.data(),static_cast<DWORD>(p.size()));if(!n||n>=p.size())fail("Locate executable");p.resize(n);return std::filesystem::path(p).parent_path();}
std::string read_file(const std::filesystem::path& p,std::size_t limit){
 std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Cannot read local file");f.seekg(0,std::ios::end);auto size=f.tellg();if(size<0||static_cast<std::uint64_t>(size)>limit)throw std::runtime_error("Local file exceeds size limit");f.seekg(0);std::string out(static_cast<std::size_t>(size),0);if(!out.empty()&&!f.read(out.data(),static_cast<std::streamsize>(out.size())))throw std::runtime_error("Local file read failed");return out;
}
static void crypto(NTSTATUS status,const char* op){if(status<0)fail(op,static_cast<DWORD>(status));}
std::string random_hex(std::size_t bytes){if(bytes>128)throw std::runtime_error("random request too large");std::vector<unsigned char> b(bytes);crypto(BCryptGenRandom(nullptr,b.data(),static_cast<ULONG>(b.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG),"Random generation");static const char hex[]="0123456789abcdef";std::string out;for(auto c:b){out+=hex[c>>4];out+=hex[c&15];}return out;}
void atomic_write(const std::filesystem::path& p,const std::string& bytes){
 auto temp=p;temp+=L"."+wide(random_hex(8))+L".tmp";
 try{Handle h(CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));if(!h)fail("Create state file");DWORD written=0;require(WriteFile(h.get(),bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr),"Write state file");if(written!=bytes.size())throw std::runtime_error("Short state write");require(FlushFileBuffers(h.get()),"Flush state file");h.reset();require(MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH),"Commit state file");}
 catch(...){DeleteFileW(temp.c_str());throw;}
}
void publish_runtime_ready(const std::filesystem::path& root){
 FILETIME created{},exited{},kernel{},user{};
 require(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user),"Read process creation time");
 ULARGE_INTEGER started{};started.LowPart=created.dwLowDateTime;started.HighPart=created.dwHighDateTime;
 atomic_write(root/"runtime.json",json::serialize(json::object{{"schema",1},{"state","running"},
  {"process_id",GetCurrentProcessId()},{"process_started_filetime",started.QuadPart}}));
}
void clear_runtime_ready(const std::filesystem::path& root) noexcept {
 try {
  if(root.empty()||!std::filesystem::exists(root/"runtime.json"))return;
  auto value=parse(read_file(root/"runtime.json")).as_object();
  if(number(value,"process_id",UINT32_MAX)==GetCurrentProcessId())std::filesystem::remove(root/"runtime.json");
 }catch(...){ }
}
std::string sha256_file(const std::filesystem::path& path){
 BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};crypto(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0),"Open SHA-256");
 try{crypto(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0),"Create SHA-256");std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Cannot open digest input");std::vector<unsigned char> buffer(1024*1024);while(f){f.read(reinterpret_cast<char*>(buffer.data()),buffer.size());auto n=f.gcount();if(n)crypto(BCryptHashData(hash,buffer.data(),static_cast<ULONG>(n),0),"Update SHA-256");}if(!f.eof())throw std::runtime_error("Digest read failed");std::array<unsigned char,32> digest{};crypto(BCryptFinishHash(hash,digest.data(),32,0),"Finish SHA-256");BCryptDestroyHash(hash);hash=nullptr;BCryptCloseAlgorithmProvider(algorithm,0);algorithm=nullptr;static const char h[]="0123456789abcdef";std::string out;for(auto c:digest){out+=h[c>>4];out+=h[c&15];}return out;}
 catch(...){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);throw;}
}
std::string protect(const std::string& value){DATA_BLOB in{static_cast<DWORD>(value.size()),reinterpret_cast<BYTE*>(const_cast<char*>(value.data()))},out{};require(CryptProtectData(&in,L"OrganizerWorker",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out),"Protect worker identity");std::string bytes(reinterpret_cast<char*>(out.pbData),out.cbData);SecureZeroMemory(out.pbData,out.cbData);LocalFree(out.pbData);return bytes;}
std::string unprotect(const std::string& bytes){DATA_BLOB in{static_cast<DWORD>(bytes.size()),reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data()))},out{};require(CryptUnprotectData(&in,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out),"Unlock identity (same Windows user required)");std::string value(reinterpret_cast<char*>(out.pbData),out.cbData);SecureZeroMemory(out.pbData,out.cbData);LocalFree(out.pbData);return value;}
static std::string base64(const std::string& s){DWORD n=0;require(CryptBinaryToStringA(reinterpret_cast<const BYTE*>(s.data()),static_cast<DWORD>(s.size()),CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,nullptr,&n),"Base64 length");std::string out(n,0);require(CryptBinaryToStringA(reinterpret_cast<const BYTE*>(s.data()),static_cast<DWORD>(s.size()),CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,out.data(),&n),"Base64 encode");out.resize(n);while(!out.empty()&&out.back()==0)out.pop_back();return out;}
class Key {
 BCRYPT_ALG_HANDLE alg_{};
public:
 BCRYPT_KEY_HANDLE key{};
 explicit Key(const std::filesystem::path& file){
 crypto(BCryptOpenAlgorithmProvider(&alg_,BCRYPT_ECDSA_P256_ALGORITHM,nullptr,0),"Open pairing identity");
 try{if(std::filesystem::exists(file)){auto b=unprotect(read_file(file));auto status=BCryptImportKeyPair(alg_,nullptr,BCRYPT_ECCPRIVATE_BLOB,&key,reinterpret_cast<PUCHAR>(b.data()),static_cast<ULONG>(b.size()),0);SecureZeroMemory(b.data(),b.size());crypto(status,"Load pairing identity");}
 else{crypto(BCryptGenerateKeyPair(alg_,&key,256,0),"Generate pairing key");crypto(BCryptFinalizeKeyPair(key,0),"Finalize pairing key");auto b=export_blob(BCRYPT_ECCPRIVATE_BLOB);atomic_write(file,protect(b));SecureZeroMemory(b.data(),b.size());}}
 catch(...){if(key)BCryptDestroyKey(key);BCryptCloseAlgorithmProvider(alg_,0);throw;}}
 ~Key(){if(key)BCryptDestroyKey(key);if(alg_)BCryptCloseAlgorithmProvider(alg_,0);}
 std::string export_blob(LPCWSTR type){ULONG n=0;crypto(BCryptExportKey(key,nullptr,type,nullptr,0,&n,0),"Export key length");std::string out(n,0);crypto(BCryptExportKey(key,nullptr,type,reinterpret_cast<PUCHAR>(out.data()),n,&n,0),"Export key");out.resize(n);return out;}
};
std::string public_pairing_key(const std::filesystem::path& p){Key k(p);return base64(k.export_blob(BCRYPT_ECCPUBLIC_BLOB));}
std::string sign_pairing_challenge(const std::filesystem::path& p,const std::string& challenge){
 Key key(p);BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};crypto(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0),"Open challenge hash");
 try{crypto(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0),"Create challenge hash");crypto(BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(challenge.data())),static_cast<ULONG>(challenge.size()),0),"Hash challenge");std::array<unsigned char,32> digest{};crypto(BCryptFinishHash(hash,digest.data(),32,0),"Finish challenge hash");BCryptDestroyHash(hash);hash=nullptr;BCryptCloseAlgorithmProvider(algorithm,0);algorithm=nullptr;ULONG n=0;crypto(BCryptSignHash(key.key,nullptr,digest.data(),32,nullptr,0,&n,0),"Signature length");std::string signature(n,0);crypto(BCryptSignHash(key.key,nullptr,digest.data(),32,reinterpret_cast<PUCHAR>(signature.data()),n,&n,0),"Sign pairing challenge");signature.resize(n);return base64(signature);}
 catch(...){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);throw;}
}
Resources available_resources(const std::filesystem::path& path,std::uint64_t reserve){MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);require(GlobalMemoryStatusEx(&memory),"Read available memory");ULARGE_INTEGER disk{};require(GetDiskFreeSpaceExW(path.c_str(),&disk,nullptr,nullptr),"Read available disk");auto free=memory.ullAvailPhys/(1024*1024);return {free>reserve?free-reserve:0,0,disk.QuadPart/(1024*1024),GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)};}
bool elevated(){HANDLE raw{};require(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&raw),"Read current token");Handle h(raw);TOKEN_ELEVATION e{};DWORD n=0;require(GetTokenInformation(h.get(),TokenElevation,&e,sizeof(e),&n),"Read privilege level");return e.TokenIsElevated!=0;}
void Log::open(){file_.reset(CreateFileW(path_.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));if(!file_)fail("Open worker log");}
void Log::write(const std::string& message){
 std::lock_guard lock(mutex_);LARGE_INTEGER size{};if(GetFileSizeEx(file_.get(),&size)&&size.QuadPart>5*1024*1024){file_.reset();auto old=path_;old+=L".1";DeleteFileW(old.c_str());MoveFileExW(path_.c_str(),old.c_str(),MOVEFILE_REPLACE_EXISTING);open();}
 std::string safe=message.substr(0,4096);for(char& c:safe)if(static_cast<unsigned char>(c)<32)c=' ';SYSTEMTIME now{};GetSystemTime(&now);char stamp[40]{};
 std::snprintf(stamp,sizeof(stamp),"%04u-%02u-%02u %02u:%02u:%02u.%03uZ",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds);
 std::string line=std::string(stamp)+" "+safe+"\r\n";DWORD n=0;require(WriteFile(file_.get(),line.data(),static_cast<DWORD>(line.size()),&n,nullptr),"Write worker log");
}
}
