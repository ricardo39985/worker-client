$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=$PSScriptRoot
$env:TEMP=$root;$env:TMP=$root;$env:PSModuleAnalysisCachePath=Join-Path $root "module-cache"
try{
 $identity=[Security.Principal.WindowsIdentity]::GetCurrent();$principal=New-Object Security.Principal.WindowsPrincipal($identity)
 if($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Capture must be normal user'}
 $process=@(Get-Process OrganizerWorkerConsole | Where-Object SessionId -eq (Get-Process -Id $PID).SessionId)
 if($process.Count -ne 1){throw 'Expected one viewer in signed-in session'}
 Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;using System.Text;
public static class WorkerConsoleCapture {
 [StructLayout(LayoutKind.Sequential)] public struct Coord { public short X,Y; }
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public short Left,Top,Right,Bottom; }
 [StructLayout(LayoutKind.Sequential)] public struct Info { public Coord Size,Cursor; public short Attributes; public Rect Window; public Coord Maximum; }
 [DllImport("kernel32.dll")] public static extern bool FreeConsole();
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool AttachConsole(uint pid);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern IntPtr CreateFileW(string path,uint access,uint share,IntPtr security,uint mode,uint flags,IntPtr template);
 [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool GetConsoleScreenBufferInfo(IntPtr h,out Info info);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern bool ReadConsoleOutputCharacterW(IntPtr h,StringBuilder text,uint length,Coord origin,out uint read);
 public static string Read(uint pid) { FreeConsole(); if(!AttachConsole(pid)) throw new Exception("AttachConsole error "+Marshal.GetLastWin32Error()); try {var h=CreateFileW("CONOUT$",0x80000000,3,IntPtr.Zero,3,0,IntPtr.Zero);Info info;if(!GetConsoleScreenBufferInfo(h,out info))throw new Exception("Console info unavailable");var result=new StringBuilder();for(short y=info.Window.Top;y<=info.Window.Bottom;y++){var text=new StringBuilder(info.Size.X);uint count;if(!ReadConsoleOutputCharacterW(h,text,(uint)info.Size.X,new Coord{X=0,Y=y},out count))throw new Exception("Read buffer failed");result.AppendLine(text.ToString().TrimEnd());}return result.ToString();}finally{FreeConsole();}}
}
'@
 $text=[WorkerConsoleCapture]::Read([uint32]$process[0].Id)
 [IO.File]::WriteAllText((Join-Path $root 'console-buffer.txt'),$text)
 if($text -notmatch 'state\s+accepting' -or $text -notmatch 'cards\s+.+idle' -or $text -notmatch 'encoders\s+cpu'){throw 'Idle table is absent from actual viewer buffer'}
 [IO.File]::WriteAllText((Join-Path $root 'console-capture-result.txt'),'CONSOLE_CAPTURE_EXIT=0; normal-user=True; session='+$process[0].SessionId)
}catch{[IO.File]::WriteAllText((Join-Path $root 'console-capture-result.txt'),('CONSOLE_CAPTURE_EXIT=1; '+$_.Exception.Message));exit 1}
exit 0
