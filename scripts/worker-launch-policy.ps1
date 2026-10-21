# Fixed owner lifecycle policy. No task creation/deletion, credential input or
# alternate command execution. Native enrollment validation is mandatory for boot.
function Assert-WorkerLaunchTask {
    param($Task,[string]$OwnerSid,[bool]$Unattended,$Enrollment)
    if([string]$Task.Principal.RunLevel -notin @('Limited','0') -or -not (Test-WorkerAccountSid -Account $Task.Principal.UserId -ExpectedSid $OwnerSid)){throw 'Existing task must retain the same LIMITED owner'}
    if($Unattended){
        if([string]$Task.Principal.LogonType -notin @('Password','1') -or $Task.TaskName -cne ('OrganizerWorker-Unattended-'+$OwnerSid)){throw 'Unattended startup requires the exact password-backed owner task; no S4U/service fallback'}
        if(-not $Enrollment -or -not $Enrollment.ok -or -not $Enrollment.enrolled -or $Enrollment.owner_sid -cne $OwnerSid -or $Enrollment.task_name -cne $Task.TaskName){throw 'Native fixed-action/ACL/profile enrollment validation required'}
    } elseif([string]$Task.Principal.LogonType -notin @('Interactive','3')){throw 'Legacy startup must remain the existing interactive owner task'}
}
function Test-WorkerLaunchReady {
    param($Status,[bool]$Unattended)
    if(-not $Status.live -or -not $Status.connected -or $Status.exiting){return $false}
    if($Unattended){return ($Status.headless -eq $true -and $Status.identity_ready -eq $true -and $Status.processing_ready -eq $true -and $Status.token_elevated -eq $false)}
    return $true
}
