$Stage='C:\Users\Danny\AppData\Local\Temp\ow-perf-finish-20261018'
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$task='OrganizerWorkerPerfCapture-20261018'
$script=Join-Path $Stage 'capture-console.ps1'
if(-not (Test-Path $script)){throw 'Capture script missing'}
$action='C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File '+$script
& schtasks.exe /Create /TN $task /TR $action /SC ONCE /ST 23:59 /RU INTERACTIVE /RL LIMITED /F
$code=$LASTEXITCODE;Write-Output "CAPTURE_TASK_CREATE_EXIT=$code";if($code){exit $code}
try{
 & schtasks.exe /Run /TN $task
 $code=$LASTEXITCODE;Write-Output "CAPTURE_TASK_RUN_EXIT=$code";if($code){exit $code}
 $result=Join-Path $Stage 'console-capture-result.txt'
 $until=[datetime]::UtcNow.AddSeconds(40)
 while(-not (Test-Path $result) -and [datetime]::UtcNow -lt $until){Start-Sleep -Milliseconds 500}
 if(-not (Test-Path $result)){throw 'Capture did not finish'}
 $text=Get-Content $result -Raw;Write-Output $text
 if(Test-Path (Join-Path $Stage 'console-buffer.txt')){Get-Content (Join-Path $Stage 'console-buffer.txt')}
 if($text -notmatch 'CONSOLE_CAPTURE_EXIT=0'){throw 'Capture failed'}
}finally{
 & schtasks.exe /Delete /TN $task /F
 $code=$LASTEXITCODE;Write-Output "CAPTURE_TASK_DELETE_EXIT=$code"
 if($code){throw 'Temporary capture task cleanup failed'}
}
exit 0
