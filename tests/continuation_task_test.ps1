param([Parameter(Mandatory=$true)][string]$SourceRoot)
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
. (Join-Path $SourceRoot 'scripts/start-headless.ps1')
$sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$policy=[pscustomobject]@{enrollment_id=('a'*32);launcher_sha256=('b'*64)}
function One($Value){$c=[pscustomobject]@{Count=1;Value=$Value};$c|Add-Member ScriptMethod Item {param($index)if($index -ne 1){throw 'Wrong COM item index'};return $this.Value};return $c}
function New-TaskSnapshot {
 $launcher=Join-Path (Join-Path (Join-Path ([Environment]::GetFolderPath('ProgramFiles')) ('OrganizerWorkerBootstrap/'+$sid)) $policy.launcher_sha256) 'launch.ps1'
 $action=[pscustomobject]@{Type=0;Path=(Join-Path ([Environment]::GetFolderPath('System')) 'WindowsPowerShell/v1.0/powershell.exe');Arguments=('-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "'+$launcher+'"');WorkingDirectory=(Split-Path $launcher)}
 $settings=[pscustomobject]@{Enabled=$true;AllowDemandStart=$true;StartWhenAvailable=$true;MultipleInstances=2;ExecutionTimeLimit='PT0S';DisallowStartIfOnBatteries=$false;StopIfGoingOnBatteries=$false;AllowHardTerminate=$false;RunOnlyIfIdle=$false;RunOnlyIfNetworkAvailable=$false;WakeToRun=$false;RestartCount=3;RestartInterval='PT1M';DeleteExpiredTaskAfter=''}
 $t=[pscustomobject]@{Name=('OrganizerWorker-Unattended-'+$sid);Enabled=$true;Security=('D:P(A;;GA;;;SY)(A;;GA;;;'+$sid+')');Definition=[pscustomobject]@{Principal=[pscustomobject]@{UserId=$sid;LogonType=1;RunLevel=0};RegistrationInfo=[pscustomobject]@{Description=('Organizer Worker owner enrollment '+$policy.enrollment_id)};Actions=(One $action);Triggers=(One ([pscustomobject]@{Type=8;Enabled=$true;Delay='PT30S';StartBoundary='';EndBoundary=''}));Settings=$settings}}
 $t|Add-Member ScriptMethod GetSecurityDescriptor {param($flags)return $this.Security}
 return $t
}
Assert-WorkerContinuationTask (New-TaskSnapshot) $policy $sid
$errors=New-Object 'Collections.Generic.List[string]'
foreach($bad in @('admin','s4u','command','provenance','inherit-only','hard-stop')){
 $t=New-TaskSnapshot
 switch($bad){'admin'{$t.Definition.Principal.RunLevel=1};'s4u'{$t.Definition.Principal.LogonType=2};'command'{$a=$t.Definition.Actions.Item(1);$a.Arguments+=' -Command forbidden'};'provenance'{$t.Definition.RegistrationInfo.Description+='foreign'};'inherit-only'{$t.Security='D:P(A;;GA;;;SY)(A;IO;GA;;;'+$sid+')'};'hard-stop'{$t.Definition.Settings.AllowHardTerminate=$true}}
 $rejected=$false;try{Assert-WorkerContinuationTask $t $policy $sid}catch{$rejected=$true}
 if(-not $rejected){$errors.Add($bad)}
}
if($errors.Count){throw ('Unsafe continuation task accepted: '+($errors -join ','))}
'CONTINUATION_TASK_PASS exact owner/Password/LUA/nonce/protected action/effective ACL/lifetime policy; NOT task registration'
