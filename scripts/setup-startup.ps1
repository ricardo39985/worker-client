function Get-WorkerStartupMode([string]$Root,[string]$Sid) {
    $path=Join-Path $Root 'unattended/enrollment.json'
    if(-not (Test-Path -LiteralPath $path)){return 'legacy'}
    if((Get-Item -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse enrollment refused'}
    $record=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json
    if($record.schema -ne 1 -or $record.owner_sid -cne $Sid -or $record.state -notin @('pending','enrolled','pairing_repair')){throw 'Existing enrollment state requires owner recovery; no startup fallback'}
    if($record.state -eq 'enrolled'){return 'unattended'}
    return 'legacy' # cancelled/pending enrollment or EXPLICIT pairing maintenance
}
function Test-WorkerEnrolledStartup([string]$Root,[string]$Sid) {
    try{
        $current=Get-Content (Join-Path $Root 'current.json') -Raw|ConvertFrom-Json
        $ctl=Join-Path $current.directory 'OrganizerWorkerCtl.exe'
        $raw=(& $ctl --json enrollment)-join "`n";if($LASTEXITCODE){return $false};$e=$raw|ConvertFrom-Json
        return ($e.ok -eq $true -and $e.enrolled -eq $true -and $e.owner_sid -ceq $Sid -and $e.task_name -ceq ('OrganizerWorker-Unattended-'+$Sid))
    }catch{return $false}
}
function Get-WorkerExistingStartupTask([string]$Sid) {
    if(-not (Get-Command Get-ScheduledTask -ErrorAction SilentlyContinue)){return $null}
    try{return Get-ScheduledTask -TaskName ('OrganizerWorker-'+$Sid) -ErrorAction Stop}
    catch{if($_.CategoryInfo.Category -eq 'ObjectNotFound'){return $null};throw 'Existing startup registration unreadable; retained without repair'}
}
function Test-WorkerStartupShortcut([string]$Root) {
    $path=Join-Path ([Environment]::GetFolderPath('Startup')) 'OrganizerWorker.lnk'
    if(-not (Test-Path -LiteralPath $path -PathType Leaf)){return $false}
    $shell=New-Object -ComObject WScript.Shell
    try{
        $shortcut=$shell.CreateShortcut($path)
        $ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
        $args='-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "'+(Join-Path $Root 'start-worker.ps1')+'"'
        return ($shortcut.TargetPath -ieq $ps -and $shortcut.Arguments -ceq $args -and $shortcut.WorkingDirectory -ieq $Root)
    }finally{[Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)|Out-Null}
}
function Register-WorkerStartup([string]$Root,[string]$Sid) {
    if((Get-WorkerStartupMode $Root $Sid) -eq 'unattended'){
        if(-not (Test-WorkerEnrolledStartup $Root $Sid)){throw 'Native owner enrollment invalid; LOCAL secure recovery required, no interactive/S4U fallback'}
        return # preserve exact password-backed task, no credential prompt/registration
    }
    $existing=Get-WorkerExistingStartupTask $Sid
    if($existing){if(-not (Test-WorkerStartupTask $Root $Sid)){throw 'Unknown pre-existing startup task retained; no overwrite/delete/fallback'};return}
    $shortcutPath=Join-Path ([Environment]::GetFolderPath('Startup')) 'OrganizerWorker.lnk'
    if(Test-Path -LiteralPath $shortcutPath){if(-not (Test-WorkerStartupShortcut $Root)){throw 'Unknown pre-existing Startup shortcut retained'};return}
    $ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $arguments='-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "'+(Join-Path $Root 'start-worker.ps1')+'"'
    $user=[Security.Principal.WindowsIdentity]::GetCurrent().Name;$created=$false
    try{
        $action=New-ScheduledTaskAction -Execute $ps -Argument $arguments
        $trigger=New-ScheduledTaskTrigger -AtLogOn -User $user
        $principal=New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Limited
        $settings=New-ScheduledTaskSettingsSet -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
        Register-ScheduledTask -TaskName ('OrganizerWorker-'+$Sid) -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description 'User-controlled Organizer worker; tray Exit stops it.'|Out-Null
        $created=$true
        if(-not (Test-WorkerStartupTask $Root $Sid)){throw 'New task readback failed; retained for owner review'}
        return
    }catch{
        if($created){throw}
        if(Get-WorkerExistingStartupTask $Sid){throw 'Concurrent/preexisting startup task retained; no fallback'}
        Write-Host '[REPAIR] Interactive Task Scheduler unavailable; creating a new owner Startup shortcut only.'
        $folder=Split-Path -Parent $shortcutPath;if(-not $folder){throw 'No owner Startup folder'}
        New-Item -ItemType Directory -Path $folder -Force|Out-Null
        if(Test-Path -LiteralPath $shortcutPath){throw 'Concurrent Startup shortcut retained'}
        $shell=New-Object -ComObject WScript.Shell
        try{$shortcut=$shell.CreateShortcut($shortcutPath);$shortcut.TargetPath=$ps;$shortcut.Arguments=$arguments;$shortcut.WorkingDirectory=$Root;$shortcut.WindowStyle=7;$shortcut.Save()}
        finally{[Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)|Out-Null}
    }
}
function Test-WorkerStartupTask([string]$Root,[string]$Sid) {
    try{
        if((Get-WorkerStartupMode $Root $Sid) -eq 'unattended'){return Test-WorkerEnrolledStartup $Root $Sid}
        $task=Get-ScheduledTask -TaskName ('OrganizerWorker-'+$Sid) -ErrorAction Stop
        $ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
        $args='-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "'+(Join-Path $Root 'start-worker.ps1')+'"'
        return (([string]$task.Principal.RunLevel -in @('Limited','0')) -and ([string]$task.Principal.LogonType -in @('Interactive','3')) -and
          (Test-WorkerAccountSid -Account ([string]$task.Principal.UserId) -ExpectedSid $Sid) -and @($task.Actions).Count -eq 1 -and
          $task.Actions[0].Execute -eq $ps -and $task.Actions[0].Arguments -ceq $args -and
          @($task.Triggers).Count -eq 1 -and (Test-WorkerAccountSid -Account ([string]$task.Triggers[0].UserId) -ExpectedSid $Sid) -and $task.Triggers[0].Enabled -and $task.Settings.Enabled)
    }catch{return $false}
}
function Test-WorkerStartup([string]$Root,[string]$Sid) {
    if((Get-WorkerStartupMode $Root $Sid) -eq 'unattended'){return Test-WorkerEnrolledStartup $Root $Sid}
    if(Test-WorkerStartupTask $Root $Sid){return $true}
    try{return Test-WorkerStartupShortcut $Root}catch{return $false}
}
