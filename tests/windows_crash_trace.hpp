#pragma once
#ifndef OW_FLEX_BASELINE
#include "platform.hpp"
#include <dbghelp.h>
#include <cstdio>
#include <cstring>
namespace ow::win {
inline wchar_t measurement_crash_path[32768]{},measurement_symbol_path[32768]{};
inline LONG WINAPI measurement_exception(EXCEPTION_POINTERS* exception){
 Handle file(CreateFileW(measurement_crash_path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
 if(!file)return EXCEPTION_EXECUTE_HANDLER;
 auto line=[&](const char* text){DWORD bytes=0;WriteFile(file.get(),text,static_cast<DWORD>(strlen(text)),&bytes,nullptr);};
 char text[4096]{};sprintf_s(text,"exception=0x%08lx address=%p thread=%lu\r\n",exception->ExceptionRecord->ExceptionCode,exception->ExceptionRecord->ExceptionAddress,GetCurrentThreadId());line(text);
 const auto process=GetCurrentProcess();SymSetOptions(SYMOPT_UNDNAME|SYMOPT_LOAD_LINES|SYMOPT_FAIL_CRITICAL_ERRORS|SYMOPT_NO_PROMPTS);
 SymInitializeW(process,measurement_symbol_path,TRUE);CONTEXT context=*exception->ContextRecord;STACKFRAME64 frame{};
 frame.AddrPC={context.Rip,0,AddrModeFlat};frame.AddrStack={context.Rsp,0,AddrModeFlat};frame.AddrFrame={context.Rbp,0,AddrModeFlat};
 for(unsigned index=0;index<48&&frame.AddrPC.Offset;++index){
  alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO)+1024]{};auto symbol=reinterpret_cast<SYMBOL_INFO*>(buffer);symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=1023;DWORD64 offset=0;
  HMODULE module{};wchar_t path[2048]{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(frame.AddrPC.Offset),&module);GetModuleFileNameW(module,path,2048);
  char name[2048]{};WideCharToMultiByte(CP_UTF8,0,path,-1,name,2048,nullptr,nullptr);
  const bool known=SymFromAddr(process,frame.AddrPC.Offset,&offset,symbol)!=FALSE;
  sprintf_s(text,"%u %s+0x%llx %s+0x%llx\r\n",index,name,frame.AddrPC.Offset-reinterpret_cast<DWORD64>(module),known?symbol->Name:"unknown",offset);line(text);
  if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,GetCurrentThread(),&frame,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr))break;
 }
 FlushFileBuffers(file.get());return EXCEPTION_EXECUTE_HANDLER;
}
inline void install_measurement_crash_trace(const std::filesystem::path& root){
 wcscpy_s(measurement_crash_path,(root/"native-crash.txt").c_str());wcscpy_s(measurement_symbol_path,executable_directory().c_str());SetUnhandledExceptionFilter(measurement_exception);
}
}
#endif
