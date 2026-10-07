#include "process.hpp"
#include <vector>
#include <algorithm>
namespace ow::win {
Child::Child(const std::filesystem::path& exe,const std::vector<std::wstring>& args,const std::filesystem::path& workspace,Resources resources,Priority p):resources_(resources){
 if(!exe.is_absolute()||!std::filesystem::is_regular_file(exe))throw std::runtime_error("Approved executable is missing or not absolute");
 job_.reset(CreateJobObjectW(nullptr,nullptr));if(!job_)fail("Create process containment");priority(p);
 SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
 Handle output(CreateFileW((workspace/"process.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));if(!output)fail("Open job output");require(SetHandleInformation(output.get(),HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT),"Permit output inheritance");
 Handle input(CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));if(!input)fail("Open null input");
 STARTUPINFOEXW si{};si.StartupInfo.cb=sizeof(si);si.StartupInfo.dwFlags=STARTF_USESTDHANDLES;si.StartupInfo.hStdInput=input.get();si.StartupInfo.hStdOutput=si.StartupInfo.hStdError=output.get();
 SIZE_T n=0;InitializeProcThreadAttributeList(nullptr,1,0,&n);std::vector<unsigned char> buffer(n);si.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buffer.data());require(InitializeProcThreadAttributeList(si.lpAttributeList,1,0,&n),"Initialize safe handle inheritance");
 HANDLE handles[]={input.get(),output.get()};
 try{require(UpdateProcThreadAttribute(si.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(handles),nullptr,nullptr),"Restrict inherited handles");std::wstring command=quote_windows_argument(exe.wstring());for(const auto& arg:args)command+=L" "+quote_windows_argument(arg);if(command.size()>30000)throw std::runtime_error("Command line exceeds limit");PROCESS_INFORMATION pi{};
 require(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_SUSPENDED|CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,workspace.c_str(),&si.StartupInfo,&pi),"Create job process");process_.reset(pi.hProcess);Handle thread(pi.hThread);
 if(!AssignProcessToJobObject(job_.get(),process_.get())){DWORD e=GetLastError();TerminateProcess(process_.get(),1);WaitForSingleObject(process_.get(),INFINITE);fail("Contain child process",e);}
 if(ResumeThread(thread.get())==static_cast<DWORD>(-1)){stop();fail("Start contained child");}
 DeleteProcThreadAttributeList(si.lpAttributeList);
 }catch(...){DeleteProcThreadAttributeList(si.lpAttributeList);throw;}
}
Child::~Child(){stop();}
void Child::priority(Priority p){
 JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION|JOB_OBJECT_LIMIT_JOB_MEMORY|JOB_OBJECT_LIMIT_PRIORITY_CLASS;
 limits.JobMemoryLimit=static_cast<SIZE_T>(resources_.ram_mb)*1024*1024;
 limits.BasicLimitInformation.PriorityClass=p==Priority::maximum?NORMAL_PRIORITY_CLASS:p==Priority::normal?NORMAL_PRIORITY_CLASS:p==Priority::low?BELOW_NORMAL_PRIORITY_CLASS:IDLE_PRIORITY_CLASS;
 require(SetInformationJobObject(job_.get(),JobObjectExtendedLimitInformation,&limits,sizeof(limits)),"Apply job memory/priority limits");
 auto cpus=GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);if(!cpus)throw std::runtime_error("Cannot detect logical CPU count");
 auto share=static_cast<DWORD>(std::min<std::uint64_t>(10000,resources_.cpu_threads*10000/cpus));if(p==Priority::low||p==Priority::backup)share=std::max<DWORD>(1,share/2);
 JOBOBJECT_CPU_RATE_CONTROL_INFORMATION cpu{};cpu.ControlFlags=JOB_OBJECT_CPU_RATE_CONTROL_ENABLE|JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;cpu.CpuRate=std::max<DWORD>(1,share);
 require(SetInformationJobObject(job_.get(),JobObjectCpuRateControlInformation,&cpu,sizeof(cpu)),"Apply CPU budget");
}
std::optional<DWORD> Child::exit_code(){if(!process_)return 1;DWORD wait=WaitForSingleObject(process_.get(),0);if(wait==WAIT_TIMEOUT)return {};if(wait!=WAIT_OBJECT_0)fail("Wait for job");DWORD code=0;require(GetExitCodeProcess(process_.get(),&code),"Read child result");return code;}
void Child::stop(){
 if(job_){
  // Keep the reservation until the entire process tree is gone, not just its
  // root process. A failed shutdown must never admit work into phantom capacity.
  TerminateJobObject(job_.get(),ERROR_CANCELLED);
  JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
  for(;;){
   if(!QueryInformationJobObject(job_.get(),JobObjectBasicAccountingInformation,&accounting,sizeof(accounting),nullptr))std::terminate();
   if(accounting.ActiveProcesses==0)break;
   Sleep(25);
  }
  job_.reset();
 }
 process_.reset();
}
}
