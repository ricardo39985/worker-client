#include "control_pipe.hpp"
#include <shellapi.h>
namespace {
using namespace ow;using namespace ow::win;
HWND window{};std::filesystem::path root;Handle viewer;json::object last;std::uint64_t generation{};
constexpr UINT Tray=WM_APP+1;constexpr int Open=100,Pause=101,Exit=102,Close=103,Priority=200,Video=300,Image=400;
json::object call(const std::string& command){return control_call(root,control_request({command}));}
void console(){
 auto s=call("status");if(viewer&&WaitForSingleObject(viewer.get(),0)==WAIT_TIMEOUT)return;
 auto exe=executable_directory()/"OrganizerWorkerConsole.exe";auto line=quote_windows_argument(exe.wstring())+L" --pid "+std::to_wstring(number(s,"pid",UINT32_MAX))+L" --data-root "+quote_windows_argument(root.wstring());
 STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION p{};require(CreateProcessW(exe.c_str(),line.data(),nullptr,nullptr,FALSE,CREATE_NEW_CONSOLE,nullptr,root.c_str(),&si,&p),"Open attached viewer");viewer.reset(p.hProcess);CloseHandle(p.hThread);
}
NOTIFYICONDATAW icon(){NOTIFYICONDATAW n{};n.cbSize=sizeof(n);n.hWnd=window;n.uID=1;n.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;n.uCallbackMessage=Tray;n.hIcon=LoadIconW(nullptr,IDI_APPLICATION);auto label=L"Organizer Worker | "+wide(last.empty()?"controller unavailable":text(last,"state",64));wcsncpy_s(n.szTip,label.c_str(),_TRUNCATE);return n;}
void patch(const char* name,const json::value& value){auto r=control_request({"config","set","--stdin"},json::serialize(json::object{{name,value}}));auto reply=control_call(root,r);if(reply.at("ok")!=true)throw std::runtime_error("Controller rejected setting");}
void menu(){
 auto state=call("status");bool paused=state.at("paused").as_bool();auto cfg=control_call(root,control_request({"config","list"}));std::string priority,video,image;
 for(const auto& row:cfg.at("settings").as_array()){const auto& r=row.as_object();auto name=text(r,"name");if(name=="operator.priority")priority=text(r,"current");else if(name=="operator.video_preference")video=text(r,"current");else if(name=="operator.image_preference")image=text(r,"current");}
 HMENU m=CreatePopupMenu(),p=CreatePopupMenu(),v=CreatePopupMenu(),i=CreatePopupMenu();if(!m||!p||!v||!i)throw std::runtime_error("Cannot create attached menu");
 AppendMenuW(m,MF_STRING,Open,L"Open live console");AppendMenuW(m,MF_STRING,Pause,paused?L"Resume new jobs":L"Pause new jobs / drain");
 const char* priorities[]={"maximum","normal","low","backup"};const char* prefs[]={"preferred","allowed","disabled"};
 for(int j=0;j<4;j++)AppendMenuW(p,MF_STRING|(priority==priorities[j]?MF_CHECKED:0),Priority+j,wide(priorities[j]).c_str());
 for(int j=0;j<3;j++){AppendMenuW(v,MF_STRING|(video==prefs[j]?MF_CHECKED:0),Video+j,wide(prefs[j]).c_str());AppendMenuW(i,MF_STRING|(image==prefs[j]?MF_CHECKED:0),Image+j,wide(prefs[j]).c_str());}
 AppendMenuW(m,MF_POPUP,reinterpret_cast<UINT_PTR>(p),L"Machine priority");AppendMenuW(m,MF_POPUP,reinterpret_cast<UINT_PTR>(v),L"Video preference");AppendMenuW(m,MF_POPUP,reinterpret_cast<UINT_PTR>(i),L"Image preference");AppendMenuW(m,MF_STRING,Exit,L"Finish jobs then exit worker");AppendMenuW(m,MF_STRING,Close,L"Close tray only (worker continues)");
 POINT at{};GetCursorPos(&at);SetForegroundWindow(window);int id=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,at.x,at.y,0,window,nullptr);DestroyMenu(m);PostMessageW(window,WM_NULL,0,0);
 if(id==Open)console();else if(id==Pause)call(paused?"resume":"pause");else if(id==Close)DestroyWindow(window);
 else if(id==Exit){if(MessageBoxW(window,L"Finish current jobs, then stop the controller? The tray is separate.",L"Exit worker",MB_YESNO|MB_ICONQUESTION)==IDYES)control_call(root,control_request({"exit","drain"}));}
 else if(id>=Priority&&id<Priority+4)patch("operator.priority",priorities[id-Priority]);else if(id>=Video&&id<Video+3)patch("operator.video_preference",prefs[id-Video]);else if(id>=Image&&id<Image+3)patch("operator.image_preference",prefs[id-Image]);
}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM,LPARAM l){
 try{
  if(msg==WM_TIMER){try{last=call("status");auto g=number(last,"viewer_generation",UINT64_MAX);if(g>generation){generation=g;console();}}catch(...){last.clear();}auto n=icon();Shell_NotifyIconW(NIM_MODIFY,&n);return 0;}
  if(msg==Tray){auto event=LOWORD(l);if(event==WM_CONTEXTMENU||event==WM_RBUTTONUP)menu();else if(event==NIN_SELECT||event==WM_LBUTTONDBLCLK)console();return 0;}
  if(msg==WM_QUERYENDSESSION)return TRUE; // NEVER forward logoff/shutdown to headless work
  if(msg==WM_ENDSESSION||msg==WM_CLOSE){DestroyWindow(h);return 0;}
  if(msg==WM_DESTROY){auto n=icon();Shell_NotifyIconW(NIM_DELETE,&n);PostQuitMessage(0);return 0;}
 }catch(...){MessageBoxW(h,L"Controller is unavailable or rejected the operation. Use the installed CLI status/start.",L"Organizer Worker",MB_OK|MB_ICONWARNING);}
 return DefWindowProcW(h,msg,0,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
 try{
  require(SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32|LOAD_LIBRARY_SEARCH_USER_DIRS),"Restrict DLL search");if(elevated())throw std::runtime_error("Normal owner tray required");
  USEROBJECTFLAGS s{};DWORD n{};if(!GetUserObjectInformationW(GetProcessWindowStation(),UOI_FLAGS,&s,sizeof(s),&n)||!(s.dwFlags&WSF_VISIBLE))return 4;
  root=data_directory();last=call("status");generation=number(last,"viewer_generation",UINT64_MAX);auto name=L"Local\\OrganizerWorker.AttachedTray."+wide(process_owner_sid(GetCurrentProcess()));Handle guard(CreateMutexW(nullptr,FALSE,name.c_str()));if(!guard||GetLastError()==ERROR_ALREADY_EXISTS)return 0;
  WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=proc;wc.lpszClassName=L"OrganizerWorker.AttachedTray";require(RegisterClassW(&wc)!=0,"Register attached tray");window=CreateWindowExW(0,wc.lpszClassName,L"Organizer Worker tray",0,0,0,0,0,nullptr,nullptr,instance,nullptr);if(!window)fail("Create attached tray");auto ni=icon();require(Shell_NotifyIconW(NIM_ADD,&ni),"Attach tray");SetTimer(window,1,500,nullptr);MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}return 0;
 }catch(...){return 4;}
}
