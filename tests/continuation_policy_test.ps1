param([Parameter(Mandatory=$true)][string]$SourceRoot)
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
. (Join-Path $SourceRoot 'scripts/start-headless.ps1')
$state=[pscustomobject]@{starts=0}
$null=Invoke-WorkerAfterExit -WaitForExit {return $true} -Activate {$state.starts++}
if($state.starts -ne 1){throw 'Verified actual old-process exit must permit exact task activation'}
$failures=New-Object 'Collections.Generic.List[string]'
foreach($condition in @('alive','unknown','wrong-type')){
 $state.starts=0
 $wait=switch($condition){'alive'{{return $false}};'unknown'{{throw 'termination outcome unavailable'}};'wrong-type'{{return 'true'}}}
 try{Invoke-WorkerAfterExit -WaitForExit $wait -Activate {$state.starts++}|Out-Null}catch{}
 if($state.starts){$failures.Add($condition+': task activated without verified old-process termination')}
}
if($failures.Count){throw ($failures -join ' | ')}
'CONTINUATION_POLICY_PASS no activation over alive/unverified/unknown exact process'
