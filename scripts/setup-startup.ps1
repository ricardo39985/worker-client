function Register-WorkerStartup([string]$Root,[string]$Sid) {
    $ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $arguments='-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "'+(Join-Path $Root 'start-worker.ps1')+'"'
    $user=[Security.Principal.WindowsIdentity]::GetCurrent().Name
    $shortcutPath=Join-Path ([Environment]::GetFolderPath('Startup')) 'OrganizerWorker.lnk'
    try {
        $action=New-ScheduledTaskAction -Execute $ps -Argument $arguments
        $trigger=New-ScheduledTaskTrigger -AtLogOn -User $user
        $principal=New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Limited
        $settings=New-ScheduledTaskSettingsSet -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
        Register-ScheduledTask -TaskName ('OrganizerWorker-'+$Sid) -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description 'User-controlled Organizer worker; tray Exit stops it.' -Force | Out-Null
        if (-not (Test-WorkerStartupTask $Root $Sid)) { throw 'Task Scheduler did not retain the requested normal-user logon task.' }
        if (Test-Path -LiteralPath $shortcutPath) { Remove-Item -LiteralPath $shortcutPath -Force }
        return
    } catch {
        Write-Host '[REPAIR] Task Scheduler registration was unavailable; installing a verified shortcut in this users Startup folder.'
        $folder=Split-Path -Parent $shortcutPath
        if (-not $folder) { throw 'Windows has no interactive Startup folder for this user.' }
        New-Item -ItemType Directory -Path $folder -Force | Out-Null
        $shell=New-Object -ComObject WScript.Shell
        try {
            $shortcut=$shell.CreateShortcut($shortcutPath)
            $shortcut.TargetPath=$ps;$shortcut.Arguments=$arguments;$shortcut.WorkingDirectory=$Root;$shortcut.WindowStyle=7
            $shortcut.Save()
        } finally { [Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell) | Out-Null }
        # Remove our incomplete task if possible. The runtime instance lock also
        # prevents duplicate workers if Windows policy blocks task removal.
        if (Get-Command Unregister-ScheduledTask -ErrorAction SilentlyContinue) { Unregister-ScheduledTask -TaskName ('OrganizerWorker-'+$Sid) -Confirm:$false -ErrorAction SilentlyContinue }
    }
}
function Test-WorkerStartupTask([string]$Root,[string]$Sid) {
    try {
        $task=Get-ScheduledTask -TaskName ('OrganizerWorker-'+$Sid) -ErrorAction Stop
        $ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
        $args='-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "'+(Join-Path $Root 'start-worker.ps1')+'"'
        return (([string]$task.Principal.RunLevel -in @('Limited','0')) -and ([string]$task.Principal.LogonType -in @('Interactive','3')) -and
          (Test-WorkerAccountSid -Account ([string]$task.Principal.UserId) -ExpectedSid $Sid) -and @($task.Actions).Count -eq 1 -and
          $task.Actions[0].Execute -eq $ps -and $task.Actions[0].Arguments -eq $args -and
          @($task.Triggers).Count -eq 1 -and (Test-WorkerAccountSid -Account ([string]$task.Triggers[0].UserId) -ExpectedSid $Sid) -and $task.Triggers[0].Enabled -and $task.Settings.Enabled)
    } catch { return $false }
}
function Test-WorkerStartup([string]$Root,[string]$Sid) {
    if (Test-WorkerStartupTask $Root $Sid) { return $true }
    try {
        $path=Join-Path ([Environment]::GetFolderPath('Startup')) 'OrganizerWorker.lnk'
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $false }
        $shell=New-Object -ComObject WScript.Shell
        try {
            $shortcut=$shell.CreateShortcut($path)
            $ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
            $args='-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "'+(Join-Path $Root 'start-worker.ps1')+'"'
            return ($shortcut.TargetPath -ieq $ps -and $shortcut.Arguments -eq $args -and $shortcut.WorkingDirectory -ieq $Root)
        } finally { [Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell) | Out-Null }
    } catch { return $false }
}
