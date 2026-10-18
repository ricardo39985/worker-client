$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=Join-Path $env:LOCALAPPDATA 'OrganizerWorker\perf-land-0771b79'
Get-ChildItem (Join-Path $root 'package') -File | Get-FileHash -Algorithm SHA256 | Select-Object @{n='File';e={Split-Path $_.Path -Leaf}},Hash | ConvertTo-Json -Compress
