$ErrorActionPreference='Stop'
$dir=Join-Path $env:LOCALAPPDATA 'Temp\ow-gpu-budget-2a209bc'
$owned=@(Get-CimInstance Win32_Process | Where-Object {$_.ExecutablePath -and $_.ExecutablePath.StartsWith($dir+'\',[StringComparison]::OrdinalIgnoreCase)})
if($owned.Count){throw 'Owned test process still running; directory retained'}
Remove-Item -LiteralPath $dir -Recurse -Force
$remaining=Test-Path -LiteralPath $dir
[pscustomobject]@{removedDirectory=$dir;testPathAfterRemoval=$remaining;ownedProcessesBeforeRemoval=$owned.Count} | ConvertTo-Json
if($remaining){exit 1};exit 0
