param([Parameter(Mandatory=$true)][string]$Bootstrap,[UInt64]$OldProcessHandle,[UInt32]$ExpectedPid,[UInt64]$ExpectedStartedTicks,[string]$Sid,[string]$Root,[string]$ApprovedImageHash,[string]$ReadyFile,[string]$AfterFile)
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
$pin=[pscustomobject]@{handle=$OldProcessHandle;pid=$ExpectedPid;started=$ExpectedStartedTicks}
. $Bootstrap
Initialize-WorkerBootstrapInterop
if($ApprovedImageHash -cnotmatch '\A[a-f0-9]{64}\z'){throw 'Test-only exact approved image hash required'}
[IO.File]::WriteAllText($ReadyFile,'public test-only waiter ready')
try{
 Invoke-WorkerAfterExit -WaitForExit {[OrganizerWorker.BootstrapLease]::WaitOwnedProcess($pin.handle,$pin.pid,$pin.started,$Sid,$Root,@($ApprovedImageHash))} -Activate {[IO.File]::WriteAllText($AfterFile,'public test-only exact process exited')}
}finally{[OrganizerWorker.BootstrapLease]::CloseInherited($pin.handle)}
'INHERITED_PROCESS_PIN_PASS actual read-only kernel handle/owner/image/creation/exit; NO task activation or credentials'
