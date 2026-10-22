#requires -Version 5.1
# Fixed bootstrap is installed ADMIN-owned outside mutable owner state. No secrets.
[CmdletBinding()]param([ValidateSet('Run','Approve','Continue')][string]$Mode='Run',[string]$Package,[string]$PackageManifestSha256,[UInt64]$OldProcessHandle,[UInt32]$ExpectedPid,[UInt64]$ExpectedStartedTicks)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
# Per-process only: never auto-import a same-owner/user-writable module elevated.
$env:PSModulePath=Join-Path $PSHOME 'Modules'
function Initialize-WorkerBootstrapInterop {
 if('OrganizerWorker.BootstrapLease' -as [type]){return}
 Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Text;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Security.Cryptography;
using Microsoft.Win32.SafeHandles;
namespace OrganizerWorker {
 public sealed class BootstrapLease : IDisposable {
  const uint ReadControl=0x20000, ReadAttributes=0x80, OpenReparse=0x200000, BackupSemantics=0x2000000;
  readonly string user,scope;readonly bool machine;readonly List<SafeFileHandle> handles=new List<SafeFileHandle>();readonly List<FileStream> streams=new List<FileStream>();
  [StructLayout(LayoutKind.Sequential)]struct Tag {public uint Attributes,ReparseTag;}
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern SafeFileHandle CreateFileW(string path,uint access,uint share,IntPtr security,uint creation,uint flags,IntPtr template);
  [DllImport("kernel32.dll",SetLastError=true)]static extern bool GetFileInformationByHandleEx(SafeFileHandle file,int kind,out Tag value,uint size);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern uint GetFinalPathNameByHandleW(SafeFileHandle file,StringBuilder value,uint count,uint flags);
  [DllImport("advapi32.dll",SetLastError=true)]static extern uint GetSecurityInfo(SafeFileHandle file,int type,uint information,out IntPtr owner,out IntPtr group,out IntPtr dacl,out IntPtr sacl,out IntPtr descriptor);
  [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool ConvertSecurityDescriptorToStringSecurityDescriptorW(IntPtr descriptor,uint revision,uint information,out IntPtr text,out uint count);
  [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool ConvertStringSecurityDescriptorToSecurityDescriptorW(string text,uint revision,out IntPtr descriptor,out uint bytes);
  [DllImport("kernel32.dll")]static extern IntPtr LocalFree(IntPtr memory);
  [DllImport("kernel32.dll",SetLastError=true)]static extern bool CloseHandle(IntPtr handle);
  static void Check(bool success){if(!success)throw new Win32Exception(Marshal.GetLastWin32Error());}
  static void Check(bool success,string operation){if(!success)throw new Win32Exception(Marshal.GetLastWin32Error(),operation);}
  public BootstrapLease(string sid,string root,bool adminOnly){user=sid;scope=Path.GetFullPath(root).TrimEnd('\\');machine=adminOnly;}
  bool Trusted(string sid){return (!machine&&sid==user)||sid=="S-1-5-18"||sid=="S-1-5-32-544"||sid=="S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464";}
  void Permissions(SafeFileHandle file,bool ancestor,string path){
   IntPtr owner,group,dacl,sacl,descriptor;uint error=GetSecurityInfo(file,1,5,out owner,out group,out dacl,out sacl,out descriptor);if(error!=0)throw new Win32Exception((int)error);
   try{IntPtr text;uint length;Check(ConvertSecurityDescriptorToStringSecurityDescriptorW(descriptor,1,5,out text,out length));string sddl;try{sddl=Marshal.PtrToStringUni(text);}finally{LocalFree(text);}var security=new RawSecurityDescriptor(sddl);
    if(security.Owner==null||!Trusted(security.Owner.Value)||security.DiscretionaryAcl==null)throw new InvalidOperationException("Untrusted bootstrap/data ownership or unrestricted DACL");
    uint writes=0x40000000|0x10000000|0x10|0x100|0x40|0x10000|0x40000|0x80000;
    // Higher shared roots may allow creating unrelated directories. Inside the
    // code/data scope ALL foreign additions are forbidden (including DLL drops).
    bool inside=String.Equals(path,scope,StringComparison.OrdinalIgnoreCase)||path.StartsWith(scope+"\\",StringComparison.OrdinalIgnoreCase);
    if(!ancestor||inside)writes|=2|4;
    foreach(GenericAce raw in security.DiscretionaryAcl){var ace=raw as CommonAce;if(ace==null)throw new InvalidOperationException("Unexpected ACL kind");if((ace.AceFlags&AceFlags.InheritOnly)!=0||ace.AceQualifier==AceQualifier.AccessDenied)continue;if(ace.AceQualifier!=AceQualifier.AccessAllowed)throw new InvalidOperationException("Unexpected ACL qualifier");if(!Trusted(ace.SecurityIdentifier.Value)&&(((uint)ace.AccessMask&writes)!=0))throw new InvalidOperationException("Foreign principal can modify bootstrap/data/ancestor");}
   }finally{LocalFree(descriptor);}
  }
  SafeFileHandle Open(string path,bool directory,bool ancestor){
   path=Path.GetFullPath(path);if(path.StartsWith("\\\\",StringComparison.Ordinal))throw new InvalidOperationException("Local paths only");
   var file=CreateFileW(path,directory?ReadControl|ReadAttributes:0x80000000|ReadControl,directory?3u:1u,IntPtr.Zero,3,OpenReparse|(directory?BackupSemantics:0),IntPtr.Zero);if(file.IsInvalid){file.Dispose();throw new Win32Exception(Marshal.GetLastWin32Error());}handles.Add(file);
   Tag tag;Check(GetFileInformationByHandleEx(file,9,out tag,8));if((tag.Attributes&0x400)!=0)throw new InvalidOperationException("Reparse state or ancestor refused");
   var name=new StringBuilder(32768);uint size=GetFinalPathNameByHandleW(file,name,(uint)name.Capacity,0);Check(size!=0&&size<name.Capacity);string actual=name.ToString();if(actual.StartsWith("\\\\?\\",StringComparison.Ordinal))actual=actual.Substring(4);if(!String.Equals(Path.GetFullPath(actual).TrimEnd('\\'),path.TrimEnd('\\'),StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("Opened path differs");
   Permissions(file,ancestor,path);return file;
  }
  public FileStream OpenFile(string path){
   path=Path.GetFullPath(path);var parents=new List<string>();for(string p=Path.GetDirectoryName(path);!String.IsNullOrEmpty(p);p=Path.GetDirectoryName(p))parents.Add(p);parents.Reverse();foreach(string parent in parents)Open(parent,true,true);
   var stream=new FileStream(Open(path,false,false),FileAccess.Read);streams.Add(stream);return stream;
  }
  public static string ReadText(FileStream stream,int maximum){if(stream.Length>maximum)throw new InvalidOperationException("Bounded bootstrap metadata exceeded");stream.Position=0;using(var reader=new StreamReader(stream,Encoding.UTF8,true,4096,true)){return reader.ReadToEnd();}}
  public static string Hash(FileStream stream){stream.Position=0;using(var sha=SHA256.Create()){string value=BitConverter.ToString(sha.ComputeHash(stream)).Replace("-","").ToLowerInvariant();stream.Position=0;return value;}}
  [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]struct Startup {public uint cb;public string reserved,desktop,title;public uint x,y,width,height,countX,countY,fill,flags;public ushort show,reserved2;public IntPtr bytes,input,output,error;}
  [StructLayout(LayoutKind.Sequential)]struct ProcessInfo {public IntPtr process,thread;public uint pid,tid;}
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool CreateProcessW(string image,StringBuilder command,IntPtr processSecurity,IntPtr threadSecurity,bool inherit,uint flags,IntPtr environment,string directory,ref Startup startup,out ProcessInfo process);
  [DllImport("kernel32.dll",SetLastError=true)]static extern uint WaitForSingleObject(IntPtr process,uint milliseconds);
  [DllImport("kernel32.dll",SetLastError=true)]static extern bool GetExitCodeProcess(IntPtr process,out uint code);
  [DllImport("user32.dll")]static extern IntPtr GetProcessWindowStation();
  [DllImport("user32.dll")]static extern IntPtr GetThreadDesktop(uint thread);
  [DllImport("kernel32.dll")]static extern uint GetCurrentThreadId();
  [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool GetUserObjectInformationW(IntPtr handle,int kind,StringBuilder value,uint bytes,out uint needed);
  static string ObjectName(IntPtr value){var text=new StringBuilder(32768);uint needed;Check(GetUserObjectInformationW(value,2,text,(uint)text.Capacity*2,out needed));return text.ToString();}
  public static int RunHeadless(string image,string root){
   var identity=WindowsIdentity.GetCurrent();if(new WindowsPrincipal(identity).IsInRole(WindowsBuiltInRole.Administrator)||identity.User.Value=="S-1-5-18"||identity.User.Value=="S-1-5-19"||identity.User.Value=="S-1-5-20")throw new InvalidOperationException("No administrative/service engine");
   var startup=new Startup();startup.cb=(uint)Marshal.SizeOf(typeof(Startup));startup.desktop=ObjectName(GetProcessWindowStation())+"\\"+ObjectName(GetThreadDesktop(GetCurrentThreadId()));ProcessInfo process;
   Check(CreateProcessW(image,new StringBuilder("\""+image+"\" --headless"),IntPtr.Zero,IntPtr.Zero,false,0x08000000,IntPtr.Zero,root,ref startup,out process));CloseHandle(process.thread);
   try{if(WaitForSingleObject(process.process,0xffffffff)!=0)throw new Win32Exception(Marshal.GetLastWin32Error());uint code;Check(GetExitCodeProcess(process.process,out code));return unchecked((int)code);}finally{CloseHandle(process.process);}
  }
  [StructLayout(LayoutKind.Sequential)]struct Attributes {public uint size;public IntPtr descriptor;[MarshalAs(UnmanagedType.Bool)]public bool inherit;}
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true,EntryPoint="CreateFileW")]static extern SafeFileHandle CreatePrivateFile(string path,uint access,uint share,ref Attributes attributes,uint creation,uint flags,IntPtr template);
  [StructLayout(LayoutKind.Sequential)]struct Time {public uint low,high;}
  [DllImport("kernel32.dll",SetLastError=true)]static extern uint GetProcessId(IntPtr process);
  [DllImport("kernel32.dll",SetLastError=true)]static extern bool GetProcessTimes(IntPtr process,out Time created,out Time exited,out Time kernel,out Time userTime);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool QueryFullProcessImageNameW(IntPtr process,uint flags,StringBuilder image,ref uint count);
  [DllImport("advapi32.dll",SetLastError=true)]static extern bool OpenProcessToken(IntPtr process,uint access,out IntPtr token);
  [DllImport("advapi32.dll",SetLastError=true)]static extern bool GetTokenInformation(IntPtr token,int kind,IntPtr buffer,uint bytes,out uint needed);
  public static bool WaitOwnedProcess(ulong inherited,uint expectedPid,ulong expectedStarted,string sid,string root,string[] approved){
   IntPtr process=new IntPtr(unchecked((long)inherited));if(inherited==0||expectedPid==0||expectedStarted==0||GetProcessId(process)!=expectedPid)throw new InvalidOperationException("Exact inherited process pin differs");
   Time created,exited,kernel,userTime;Check(GetProcessTimes(process,out created,out exited,out kernel,out userTime),"Read pinned process creation time");if((((ulong)created.high<<32)|created.low)!=expectedStarted)throw new InvalidOperationException("Pinned process creation differs");
   IntPtr token;Check(OpenProcessToken(process,8,out token),"Open pinned process token");
   try{uint needed;GetTokenInformation(token,1,IntPtr.Zero,0,out needed);if(needed==0||needed>8192)throw new InvalidOperationException("Owner token unavailable");IntPtr data=Marshal.AllocHGlobal((int)needed);
    try{Check(GetTokenInformation(token,1,data,needed,out needed),"Read pinned process owner token");if(new SecurityIdentifier(Marshal.ReadIntPtr(data)).Value!=sid)throw new InvalidOperationException("Pinned process owner differs");IntPtr elevation=Marshal.AllocHGlobal(4);try{uint returned;Check(GetTokenInformation(token,20,elevation,4,out returned),"Read pinned process elevation");if(returned!=4||Marshal.ReadInt32(elevation)!=0)throw new InvalidOperationException("No elevated legacy controller");}finally{Marshal.FreeHGlobal(elevation);}}finally{Marshal.FreeHGlobal(data);}
   }finally{CloseHandle(token);}
   var image=new StringBuilder(32768);uint length=(uint)image.Capacity;Check(QueryFullProcessImageNameW(process,0,image,ref length),"Read pinned process image");string path=Path.GetFullPath(image.ToString());string prefix=Path.GetFullPath(Path.Combine(root,"versions"))+"\\";
   if(!path.StartsWith(prefix,StringComparison.OrdinalIgnoreCase)||!String.Equals(Path.GetFileName(path),"OrganizerWorker.exe",StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("Pinned process is not the installed worker");
   using(var lease=new BootstrapLease(sid,root,false)){string hash=Hash(lease.OpenFile(path));if(Array.IndexOf(approved,hash)<0)throw new InvalidOperationException("Pinned image not approved");uint result=WaitForSingleObject(process,1800000);if(result==0)return true;if(result==258)return false;throw new Win32Exception(Marshal.GetLastWin32Error());}
  }
  public static void CloseInherited(ulong value){if(value!=0)CloseHandle(new IntPtr(unchecked((long)value)));}
  public static void ValidateTaskSecurity(string sddl,string sid){
   var descriptor=new RawSecurityDescriptor(sddl);if(descriptor.DiscretionaryAcl==null)throw new InvalidOperationException("Task has no restrictive DACL");bool owner=false,system=false;
   foreach(GenericAce raw in descriptor.DiscretionaryAcl){var ace=raw as CommonAce;if(ace==null||ace.AceQualifier!=AceQualifier.AccessAllowed||ace.AceFlags!=AceFlags.None)throw new InvalidOperationException("Unexpected task ACE/flags");uint mask=(uint)ace.AccessMask;if((mask&0x10000000)==0&&(mask&0x1f01ff)!=0x1f01ff)throw new InvalidOperationException("Ineffective task access");if(ace.SecurityIdentifier.Value==sid)owner=true;else if(ace.SecurityIdentifier.Value=="S-1-5-18")system=true;else throw new InvalidOperationException("Foreign task access");}
   if(!owner||!system)throw new InvalidOperationException("Missing effective owner/System task access");
  }
  public static void WritePolicy(string directory,string sid,string json){
   string temporary=Path.Combine(directory,"approval-"+Guid.NewGuid().ToString("N")+".tmp");IntPtr descriptor;uint length;Check(ConvertStringSecurityDescriptorToSecurityDescriptorW("O:BAG:BAD:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;GRGX;;;"+sid+")",1,out descriptor,out length));
   try{var attributes=new Attributes();attributes.size=(uint)Marshal.SizeOf(typeof(Attributes));attributes.descriptor=descriptor;using(var handle=CreatePrivateFile(temporary,0x40000000,0,ref attributes,1,0x80000000,IntPtr.Zero)){if(handle.IsInvalid)throw new Win32Exception(Marshal.GetLastWin32Error());using(var output=new FileStream(handle,FileAccess.Write)){byte[] bytes=new UTF8Encoding(false).GetBytes(json);output.Write(bytes,0,bytes.Length);output.Flush(true);}}string target=Path.Combine(directory,"enrollment.json");if(File.Exists(target))File.Replace(temporary,target,null);else File.Move(temporary,target);
   }catch{if(File.Exists(temporary))File.Delete(temporary);throw;}finally{LocalFree(descriptor);}
  }
  public void Dispose(){foreach(var stream in streams)stream.Dispose();foreach(var handle in handles)handle.Dispose();streams.Clear();handles.Clear();}
 }
}
'@
}
function Invoke-WorkerVerifiedImage {
 param([string]$Root,[string[]]$ApprovedHashes,[string]$OwnerSid,[scriptblock]$Launch)
 Initialize-WorkerBootstrapInterop
 $lease=New-Object OrganizerWorker.BootstrapLease($OwnerSid,$Root,$false)
 try{
  $manifest=[OrganizerWorker.BootstrapLease]::ReadText($lease.OpenFile((Join-Path $Root 'current.json')),262144)|ConvertFrom-Json
  if($manifest.schema -ne 1 -or [string]$manifest.sha256 -cnotmatch '\A[a-f0-9]{64}\z'){throw 'Invalid selected image manifest'}
  $directory=[IO.Path]::GetFullPath([string]$manifest.directory);$prefix=[IO.Path]::GetFullPath((Join-Path $Root 'versions'))+[IO.Path]::DirectorySeparatorChar
  if(-not $directory.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Version escaped owner root'}
  $image=Join-Path $directory 'OrganizerWorker.exe';$hash=[OrganizerWorker.BootstrapLease]::Hash($lease.OpenFile($image))
  if($hash -cne $manifest.sha256 -or $ApprovedHashes -cnotcontains $hash){throw 'Image not explicitly approved outside mutable manifest'}
  & $Launch $image
 }finally{$lease.Dispose()}
}
function Invoke-WorkerHeadlessLifetime {
 param([scriptblock]$Launch)
 do{$code=[int](& $Launch)}while($code -eq 75)
 return $code
}
function Invoke-WorkerAfterExit {
 param([scriptblock]$WaitForExit,[scriptblock]$Activate)
 $terminated=& $WaitForExit
 if($terminated -isnot [bool] -or -not $terminated){throw 'Exact old-process termination not established; no replacement'}
 & $Activate
}
function Get-WorkerProtectedPolicy {
 param([string]$Sid)
 Initialize-WorkerBootstrapInterop
 $anchor=Join-Path ([Environment]::GetFolderPath('ProgramFiles')) ('OrganizerWorkerBootstrap/'+$Sid)
 $lease=New-Object OrganizerWorker.BootstrapLease($Sid,$anchor,$true)
 try{
  $policy=[OrganizerWorker.BootstrapLease]::ReadText($lease.OpenFile((Join-Path $anchor 'enrollment.json')),2097152)|ConvertFrom-Json
  if($policy.schema -ne 1 -or $policy.owner_sid -cne $Sid -or $policy.enrollment_id -cnotmatch '\A[a-f0-9]{32}\z' -or $policy.launcher_sha256 -cnotmatch '\A[a-f0-9]{64}\z'){throw 'Protected enrollment provenance differs'}
  $code=Join-Path (Join-Path $anchor $policy.launcher_sha256) 'launch.ps1'
  if([IO.Path]::GetFullPath($code) -ine [IO.Path]::GetFullPath($PSCommandPath) -or [OrganizerWorker.BootstrapLease]::Hash($lease.OpenFile($code)) -cne $policy.launcher_sha256){throw 'Only the fixed protected bootstrap may execute'}
  return $policy
 }finally{$lease.Dispose()}
}
function Approve-WorkerBootstrapPackage {
 param([string]$Sid,[string]$Directory,[string]$ManifestHash)
 if($ManifestHash -cnotmatch '\A[a-f0-9]{64}\z'){throw 'Independent reviewed package-manifest SHA256 required'}
 $policy=Get-WorkerProtectedPolicy $Sid
 $lease=New-Object OrganizerWorker.BootstrapLease($Sid,$Directory,$false)
 try{
  $stream=$lease.OpenFile((Join-Path $Directory 'package.json'));if([OrganizerWorker.BootstrapLease]::Hash($stream) -cne $ManifestHash){throw 'Package manifest differs from explicit approval'}
  $manifest=[OrganizerWorker.BootstrapLease]::ReadText($stream,262144)|ConvertFrom-Json
  $names=@('OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe','OrganizerWorkerCtl.exe','OrganizerWorkerTray.exe','cli.md','start-headless.ps1')
  if($manifest.schema -ne 2 -or $manifest.revision -cnotmatch '\A[a-f0-9]{40}\z' -or $manifest.source_tree -cnotmatch '\A[a-f0-9]{40}\z' -or @($manifest.files.PSObject.Properties).Count -ne $names.Count){throw 'Reviewed schema2 code-only package required'}
  foreach($name in $names){if($name -cnotin @($manifest.files.PSObject.Properties.Name) -or $manifest.files.$name -cnotmatch '\A[a-f0-9]{64}\z' -or [OrganizerWorker.BootstrapLease]::Hash($lease.OpenFile((Join-Path $Directory $name))) -cne $manifest.files.$name){throw 'Approved package missing/changed companion'}}
  if($manifest.files.'start-headless.ps1' -cne $policy.launcher_sha256){throw 'Bootstrap policy change requires secure LOCAL owner migration; current engine retained'}
  $anchor=Join-Path ([Environment]::GetFolderPath('ProgramFiles')) ('OrganizerWorkerBootstrap/'+$Sid)
  $lock=[IO.File]::Open((Join-Path $anchor 'approval.lock'),[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
  try{
   $policy=Get-WorkerProtectedPolicy $Sid
   $policy.approved_packages|Add-Member -NotePropertyName $manifest.files.'OrganizerWorker.exe' -NotePropertyValue ([pscustomobject]@{files=$manifest.files;source=$manifest.revision;manifest_sha256=$ManifestHash}) -Force
   [OrganizerWorker.BootstrapLease]::WritePolicy($anchor,$Sid,($policy|ConvertTo-Json -Depth 32))
  }finally{$lock.Dispose()}
  'PROTECTED_CODE_HASH_APPROVED_NO_ENGINE_TASK_OR_CREDENTIAL_EXECUTED=True'
 }finally{$lease.Dispose()}
}
function Assert-WorkerRootEnrollment {
 param([string]$Sid,$Policy)
 $root=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'OrganizerWorker'
 $lease=New-Object OrganizerWorker.BootstrapLease($Sid,$root,$false)
 try{$enrollment=[OrganizerWorker.BootstrapLease]::ReadText($lease.OpenFile((Join-Path $root 'unattended/enrollment.json')),262144)|ConvertFrom-Json}finally{$lease.Dispose()}
 if($enrollment.schema -ne 1 -or $enrollment.state -cne 'enrolled' -or $enrollment.owner_sid -cne $Sid -or $enrollment.task_name -cne ('OrganizerWorker-Unattended-'+$Sid) -or $enrollment.enrollment_id -cne $Policy.enrollment_id){throw 'Existing protected enrollment/profile required'}
 return $root
}
function Assert-WorkerContinuationTask {
 param($Task,$Policy,[string]$Sid)
 Initialize-WorkerBootstrapInterop
 $definition=$Task.Definition;$principal=$definition.Principal
 $account=[string]$principal.UserId
 $accountSid=if($account -match '\AS-1-'){$account}else{(New-Object Security.Principal.NTAccount($account)).Translate([Security.Principal.SecurityIdentifier]).Value}
 if($Task.Name -cne ('OrganizerWorker-Unattended-'+$Sid) -or -not $Task.Enabled -or $accountSid -cne $Sid -or $principal.LogonType -ne 1 -or $principal.RunLevel -ne 0 -or $definition.RegistrationInfo.Description -cne ('Organizer Worker owner enrollment '+$Policy.enrollment_id)){throw 'Existing exact Password/LIMITED owner task required'}
 [OrganizerWorker.BootstrapLease]::ValidateTaskSecurity($Task.GetSecurityDescriptor(4),$Sid)
 if($definition.Actions.Count -ne 1 -or $definition.Triggers.Count -ne 1){throw 'Task action/trigger policy differs'}
 $action=$definition.Actions.Item(1);$trigger=$definition.Triggers.Item(1)
 $launcher=Join-Path (Join-Path (Join-Path ([Environment]::GetFolderPath('ProgramFiles')) ('OrganizerWorkerBootstrap/'+$Sid)) $Policy.launcher_sha256) 'launch.ps1'
 $taskHostPath=Join-Path ([Environment]::GetFolderPath('System')) 'WindowsPowerShell/v1.0/powershell.exe'
 $arguments='-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "'+$launcher+'"'
 if($action.Type -ne 0 -or [IO.Path]::GetFullPath($action.Path) -ine [IO.Path]::GetFullPath($taskHostPath) -or $action.Arguments -cne $arguments -or [IO.Path]::GetFullPath($action.WorkingDirectory) -ine [IO.Path]::GetFullPath((Split-Path $launcher)) -or $trigger.Type -ne 8 -or -not $trigger.Enabled -or $trigger.Delay -cne 'PT30S' -or $trigger.StartBoundary -or $trigger.EndBoundary){throw 'Fixed protected task action/boot policy differs'}
 $settings=$definition.Settings
 if(-not $settings.Enabled -or -not $settings.AllowDemandStart -or -not $settings.StartWhenAvailable -or $settings.MultipleInstances -ne 2 -or $settings.ExecutionTimeLimit -cne 'PT0S' -or $settings.DisallowStartIfOnBatteries -or $settings.StopIfGoingOnBatteries -or $settings.AllowHardTerminate -or $settings.RunOnlyIfIdle -or $settings.RunOnlyIfNetworkAvailable -or $settings.WakeToRun -or $settings.RestartCount -ne 3 -or $settings.RestartInterval -cne 'PT1M' -or $settings.DeleteExpiredTaskAfter){throw 'Task lifecycle/power/retry policy differs'}
}
if($MyInvocation.InvocationName -eq '.'){return}
# Advanced finite parameter binding rejects unsupported arguments before execution.
$identity=[Security.Principal.WindowsIdentity]::GetCurrent();$sid=$identity.User.Value;$admin=(New-Object Security.Principal.WindowsPrincipal($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if($sid -in @('S-1-5-18','S-1-5-19','S-1-5-20')){throw 'Existing human owner required, never a service account'}
if($Mode -eq 'Approve'){
 if(-not $admin){throw 'Explicit elevated SAME owner package approval required; no automatic elevation'}
 Approve-WorkerBootstrapPackage $sid $Package $PackageManifestSha256
 exit 0
}
if($admin){throw 'Headless engine requires normal LIMITED owner'}
if($Mode -eq 'Continue'){
 $policy=Get-WorkerProtectedPolicy $sid;$root=Assert-WorkerRootEnrollment $sid $policy
 try{Invoke-WorkerAfterExit -WaitForExit {[OrganizerWorker.BootstrapLease]::WaitOwnedProcess($OldProcessHandle,$ExpectedPid,$ExpectedStartedTicks,$sid,$root,@($policy.approved_packages.PSObject.Properties.Name))} -Activate {
   $policy=Get-WorkerProtectedPolicy $sid;$root=Assert-WorkerRootEnrollment $sid $policy
   # Do not activate over any unobserved controller; never create the lock.
   $guard=[IO.File]::Open((Join-Path $root 'instance.lock'),[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None);$guard.Dispose()
   $scheduler=New-Object -ComObject 'Schedule.Service';$scheduler.Connect();$task=$scheduler.GetFolder('\').GetTask('OrganizerWorker-Unattended-'+$sid)
   Assert-WorkerContinuationTask $task $policy $sid
   $null=$task.Run($null)
  }}finally{[OrganizerWorker.BootstrapLease]::CloseInherited($OldProcessHandle)}
 exit 0
}
$code=Invoke-WorkerHeadlessLifetime -Launch {
 $identity=[Security.Principal.WindowsIdentity]::GetCurrent();$sid=$identity.User.Value;$root=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'OrganizerWorker'
 $policy=Get-WorkerProtectedPolicy $sid
 $root=Assert-WorkerRootEnrollment $sid $policy
 Invoke-WorkerVerifiedImage -Root $root -ApprovedHashes @($policy.approved_packages.PSObject.Properties.Name) -OwnerSid $sid -Launch {
  param($image)
  [OrganizerWorker.BootstrapLease]::RunHeadless($image,(Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'OrganizerWorker'))
 }
}
exit $code
