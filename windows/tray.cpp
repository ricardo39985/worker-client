#include "worker.hpp"
#include <shellapi.h>
#include <memory>
#include <algorithm>
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
namespace {
using namespace ow;using namespace ow::win;
constexpr UINT TrayMessage=WM_APP+1,OpenViewer=WM_APP+2;
constexpr int Open=100,Pause=101,Logs=102,Exit=103,PriorityBase=200,VideoBase=300,ImageBase=400;
std::unique_ptr<Worker> worker;std::filesystem::path root;Handle viewer;HWND window{};UINT taskbarCreated{};
void console(){
 if(viewer&&WaitForSingleObject(viewer.get(),0)==WAIT_TIMEOUT)return;
 auto exe=executable_directory()/"OrganizerWorkerConsole.exe";if(!std::filesystem::is_regular_file(exe))throw std::runtime_error("Console viewer is missing; rerun setup");
 auto command=quote_windows_argument(exe.wstring())+L" --pid "+std::to_wstring(GetCurrentProcessId())+L" --data-root "+quote_windows_argument(root.wstring());STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
 require(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NEW_CONSOLE,nullptr,root.c_str(),&si,&pi),"Open live console");viewer.reset(pi.hProcess);CloseHandle(pi.hThread);
}
NOTIFYICONDATAW icon(){NOTIFYICONDATAW n{};n.cbSize=sizeof(n);n.hWnd=window;n.uID=1;n.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;n.uCallbackMessage=TrayMessage;n.hIcon=LoadIconW(nullptr,IDI_APPLICATION);std::wstring title=L"Organizer Worker | "+wide(worker?worker->status():"STARTING");wcsncpy_s(n.szTip,title.c_str(),_TRUNCATE);return n;}
void add_icon(){auto n=icon();require(Shell_NotifyIconW(NIM_ADD,&n),"Create worker tray icon");n.uVersion=NOTIFYICON_VERSION_4;require(Shell_NotifyIconW(NIM_SETVERSION,&n),"Set tray version");}
void menu(){
 HMENU m=CreatePopupMenu(),priority=CreatePopupMenu(),video=CreatePopupMenu(),image=CreatePopupMenu();if(!m||!priority||!video||!image)throw std::runtime_error("Cannot create tray menu");
 auto s=wide(worker->status())+L" | "+std::to_wstring(worker->active_count())+L" jobs";AppendMenuW(m,MF_STRING|MF_DISABLED,0,s.c_str());AppendMenuW(m,MF_SEPARATOR,0,nullptr);
 AppendMenuW(m,MF_STRING,Open,L"Open live console");AppendMenuW(m,MF_STRING,Pause,worker->paused()?L"Resume new jobs":L"Pause new jobs / drain");
 const wchar_t* names[]={L"Maximum",L"Normal",L"Low",L"Backup only"};for(int i=0;i<4;++i)AppendMenuW(priority,MF_STRING|(static_cast<int>(worker->priority())==i?MF_CHECKED:0),PriorityBase+i,names[i]);AppendMenuW(m,MF_POPUP,reinterpret_cast<UINT_PTR>(priority),L"Machine priority");
 const wchar_t* prefs[]={L"Preferred",L"Allowed",L"Disabled"};for(int i=0;i<3;++i){AppendMenuW(video,MF_STRING|(static_cast<int>(worker->preference(true))==i?MF_CHECKED:0),VideoBase+i,prefs[i]);AppendMenuW(image,MF_STRING|(static_cast<int>(worker->preference(false))==i?MF_CHECKED:0),ImageBase+i,prefs[i]);}
 AppendMenuW(m,MF_POPUP,reinterpret_cast<UINT_PTR>(video),L"Video conversion preference");AppendMenuW(m,MF_POPUP,reinterpret_cast<UINT_PTR>(image),L"Image conversion preference");AppendMenuW(m,MF_STRING,Logs,L"Open data and logs folder");AppendMenuW(m,MF_SEPARATOR,0,nullptr);AppendMenuW(m,MF_STRING,Exit,L"Exit worker...");
 POINT p{};GetCursorPos(&p);SetForegroundWindow(window);auto id=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,window,nullptr);DestroyMenu(m);PostMessageW(window,WM_NULL,0,0);
 if(id==Open)console();else if(id==Pause)worker->pause(!worker->paused());else if(id==Logs)ShellExecuteW(window,L"open",root.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
 else if(id>=PriorityBase&&id<PriorityBase+4)worker->priority(static_cast<Priority>(id-PriorityBase));
 else if(id>=VideoBase&&id<VideoBase+3)worker->preference(true,static_cast<Preference>(id-VideoBase));
 else if(id>=ImageBase&&id<ImageBase+3)worker->preference(false,static_cast<Preference>(id-ImageBase));
 else if(id==Exit){if(worker->active_count()==0){worker->exit(false);return;}int answer=MessageBoxW(window,L"Yes: finish current jobs, then exit.\n\nNo: stop current jobs and report them for retry, then exit.\n\nCancel: keep the worker running.",L"Exit Organizer Worker",MB_YESNOCANCEL|MB_ICONQUESTION);if(answer!=IDCANCEL)worker->exit(answer==IDNO);}
}
LRESULT CALLBACK procedure(HWND h,UINT message,WPARAM w,LPARAM l){
 try{
 if(message==taskbarCreated){add_icon();return 0;}
 if(message==OpenViewer){console();return 0;}
 if(message==TrayMessage){auto event=LOWORD(l);if(event==WM_CONTEXTMENU||event==WM_RBUTTONUP)menu();else if(event==NIN_SELECT||event==WM_LBUTTONDBLCLK)console();return 0;}
 if(message==WM_TIMER){auto n=icon();Shell_NotifyIconW(NIM_MODIFY,&n);if(worker->ready_to_exit())DestroyWindow(h);return 0;}
 if(message==WM_CLOSE){console();return 0;}// Only the explicit tray Exit requests a normal shutdown.
 if(message==WM_QUERYENDSESSION){worker->exit(true);return TRUE;}
 if(message==WM_ENDSESSION&&w){DestroyWindow(h);return 0;}
 if(message==WM_DESTROY){auto n=icon();Shell_NotifyIconW(NIM_DELETE,&n);PostQuitMessage(0);return 0;}
 }catch(const std::exception& e){MessageBoxW(h,wide(e.what()).c_str(),L"Organizer Worker",MB_OK|MB_ICONERROR);}
 return DefWindowProcW(h,message,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
 try{
  require(SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32|LOAD_LIBRARY_SEARCH_USER_DIRS),"Restrict DLL search");
  if(elevated())throw std::runtime_error("Run the worker as your normal Windows user, not Administrator. Setup can install build tools separately.");
  USEROBJECTFLAGS station{};DWORD length=0;if(!GetUserObjectInformationW(GetProcessWindowStation(),UOI_FLAGS,&station,sizeof(station),&length)||!(station.dwFlags&WSF_VISIBLE))throw std::runtime_error("No interactive Windows desktop. SSH can install/update; launch the worker from the signed-in desktop or its interactive logon task.");
  root=data_directory();std::filesystem::create_directories(root);Handle lock(CreateFileW((root/"instance.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
  if(!lock){if(HWND other=FindWindowW(L"OrganizerWorker.Tray",nullptr))PostMessageW(other,OpenViewer,0,0);else MessageBoxW(nullptr,L"A worker already owns this user's state directory. Use its tray icon.",L"Organizer Worker",MB_OK);return 0;}
  auto cfg=load_config(root);std::filesystem::create_directories(root/"jobs");std::filesystem::create_directories(root/"probes");worker=std::make_unique<Worker>(std::move(cfg));
  WNDCLASSW wc{};wc.lpfnWndProc=procedure;wc.hInstance=instance;wc.lpszClassName=L"OrganizerWorker.Tray";if(!RegisterClassW(&wc))fail("Register tray window");taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
  window=CreateWindowExW(0,wc.lpszClassName,L"Organizer Worker",WS_OVERLAPPED,0,0,0,0,nullptr,nullptr,instance,nullptr);if(!window)fail("Create tray controller");add_icon();if(!SetTimer(window,1,500,nullptr))fail("Start tray status timer");console();
  MSG message{};BOOL result;while((result=GetMessageW(&message,nullptr,0,0))>0){TranslateMessage(&message);DispatchMessageW(&message);}worker.reset();if(result<0)fail("Windows message loop");return 0;
 }catch(const std::exception& e){worker.reset();MessageBoxW(nullptr,wide(e.what()).c_str(),L"Organizer Worker startup failed",MB_OK|MB_ICONERROR);return 1;}
}
