#include "bootstrap_anchor.hpp"
#include "owner_state.hpp"
#include "control_pipe.hpp"
#include "ow/control.hpp"
#include "ow/protocol.hpp"
#include <shlobj.h>
#include <sddl.h>
#include <array>
namespace ow::win {
namespace {
const std::array<const char*,7> names={"OrganizerWorker.exe","OrganizerWorkerConsole.exe","OrganizerWorkerDoctor.exe","OrganizerWorkerCtl.exe","OrganizerWorkerTray.exe","cli.md","start-headless.ps1"};
std::string user(){return process_owner_sid(GetCurrentProcess());}
struct Descriptor {
 PSECURITY_DESCRIPTOR value{};
 explicit Descriptor(bool directory){auto inherit=directory?L"OICI":L"";auto sid=wide(user());auto s=L"O:BAG:BAD:P(A;"+std::wstring(inherit)+L";FA;;;SY)(A;"+inherit+L";FA;;;BA)(A;"+inherit+L";GRGX;;;"+sid+L")";require(ConvertStringSecurityDescriptorToSecurityDescriptorW(s.c_str(),SDDL_REVISION_1,&value,nullptr),"Create machine bootstrap permissions");}
 ~Descriptor(){if(value)LocalFree(value);}
};
void directory(const std::filesystem::path& path){
 if(std::filesystem::exists(path)){verify_protected_bootstrap_path(path);return;}
 auto parent=path.parent_path();if(parent==path||parent.empty())throw ControlFailure(4,"Missing trusted machine bootstrap parent");directory(parent);
 Descriptor sd(true);SECURITY_ATTRIBUTES attributes{sizeof(attributes),sd.value,FALSE};
 require(CreateDirectoryW(path.c_str(),&attributes),"Create new admin-owned bootstrap directory");verify_protected_bootstrap_path(path);
}
void create_file(const std::filesystem::path& path,const std::string& bytes){
 Descriptor sd(false);SECURITY_ATTRIBUTES attributes{sizeof(attributes),sd.value,FALSE};
 Handle file(CreateFileW(path.c_str(),GENERIC_WRITE|READ_CONTROL,0,&attributes,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));if(!file)fail("Create NEW admin-owned bootstrap file");
 std::size_t offset=0;while(offset<bytes.size()){DWORD written{};require(WriteFile(file.get(),bytes.data()+offset,static_cast<DWORD>(bytes.size()-offset),&written,nullptr),"Write bootstrap metadata");if(!written)throw ControlFailure(4,"Bootstrap metadata write incomplete");offset+=written;}
 require(FlushFileBuffers(file.get()),"Flush bootstrap metadata");file.reset();verify_protected_bootstrap_path(path);
}
void policy_write(const std::filesystem::path& path,const json::object& value){
 const auto temporary=path.parent_path()/("approval-"+random_hex(16)+".tmp");
 create_file(temporary,json::serialize(value));
 try{require(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH),"Commit protected bootstrap approval");}catch(...){DeleteFileW(temporary.c_str());throw;}
 verify_protected_bootstrap_path(path);
}
void shape(const json::object& value){
 if(number(value,"schema",1)!=1||text(value,"owner_sid",256)!=user()||text(value,"launcher_sha256",64)!=OW_HEADLESS_LAUNCHER_SHA256||!value.at("approved_packages").is_object())throw ControlFailure(4,"Protected bootstrap policy differs; local secure migration review required");
 auto id=text(value,"enrollment_id",32);if(id.size()!=32||id.find_first_not_of("0123456789abcdef")!=std::string::npos)throw ControlFailure(4,"Protected bootstrap provenance invalid");
}
}
std::filesystem::path bootstrap_anchor_directory(){
 PWSTR raw{};auto result=SHGetKnownFolderPath(FOLDERID_ProgramFiles,0,nullptr,&raw);if(FAILED(result))throw ControlFailure(4,"Cannot locate native protected Program Files");
 std::filesystem::path base(raw);CoTaskMemFree(raw);return base/"OrganizerWorkerBootstrap"/wide(user());
}
std::filesystem::path bootstrap_launcher_path(){return bootstrap_anchor_directory()/OW_HEADLESS_LAUNCHER_SHA256/"launch.ps1";}
json::object read_bootstrap_policy(){
 auto path=bootstrap_anchor_directory()/"enrollment.json";verify_protected_bootstrap_path(path);
 auto policy=parse(read_file(path,2*1024*1024)).as_object();shape(policy);
 auto launcher=bootstrap_launcher_path();verify_protected_bootstrap_path(launcher);if(sha256_file(launcher)!=OW_HEADLESS_LAUNCHER_SHA256)throw ControlFailure(4,"Protected bootstrap code differs");return policy;
}
void install_bootstrap_anchor(const std::filesystem::path& selected,const std::string& id){
 if(!elevated())throw ControlFailure(4,"Protected bootstrap installation requires explicitly elevated same-owner enrollment, not an admin engine");
 if(id.size()!=32||id.find_first_not_of("0123456789abcdef")!=std::string::npos)throw ControlFailure(4,"Enrollment provenance invalid");
 auto source=selected/"start-headless.ps1";verify_owner_state_path(source);if(sha256_file(source)!=OW_HEADLESS_LAUNCHER_SHA256)throw ControlFailure(4,"Only the compiled reviewed launcher can be installed");
 const auto base=bootstrap_anchor_directory();directory(base);directory(bootstrap_launcher_path().parent_path());
 if(std::filesystem::exists(bootstrap_launcher_path())){verify_protected_bootstrap_path(bootstrap_launcher_path());if(sha256_file(bootstrap_launcher_path())!=OW_HEADLESS_LAUNCHER_SHA256)throw ControlFailure(4,"Unknown protected launcher retained for review");}
 else create_file(bootstrap_launcher_path(),read_file(source));
 auto file=base/"enrollment.json";json::object policy;
 if(std::filesystem::exists(file)){policy=read_bootstrap_policy();if(text(policy,"enrollment_id",32)!=id)throw ControlFailure(4,"Existing machine bootstrap belongs to another enrollment transaction");}
 else policy={{"schema",1},{"owner_sid",user()},{"enrollment_id",id},{"launcher_sha256",OW_HEADLESS_LAUNCHER_SHA256},{"approved_packages",json::object{}}};
 json::object files;for(const auto* name:names){auto path=selected/name;verify_owner_state_path(path);files[name]=sha256_file(path);}
 auto worker=std::string(files.at("OrganizerWorker.exe").as_string());policy.at("approved_packages").as_object()[worker]=json::object{{"files",std::move(files)},{"source",OW_BUILD_REVISION}};
 policy_write(file,policy);
}
}
