$ErrorActionPreference='Stop'
$dir=Join-Path $env:LOCALAPPDATA 'Temp\ow-gpu-budget-2a209bc'
if(Test-Path -LiteralPath $dir){throw 'Measurement directory already exists'}
$children=@(Get-CimInstance Win32_Process | Where-Object {$_.Name -in @('ffmpeg.exe','ow_gpu_measurements.exe')})
if($children.Count){throw 'FFmpeg work currently present; defer measurement'}
$root=Join-Path $env:LOCALAPPDATA 'OrganizerWorker'
$cfg=Get-Content (Join-Path $root 'worker.json') -Raw | ConvertFrom-Json
$expected='f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4'
if((Get-FileHash -LiteralPath $cfg.ffmpeg_path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expected){throw 'Installed FFmpeg pin mismatch'}
New-Item -ItemType Directory -Path $dir | Out-Null
Copy-Item -LiteralPath $cfg.ffmpeg_path -Destination (Join-Path $dir 'ffmpeg.exe')
$workers=@(Get-CimInstance Win32_Process | Where-Object {$_.Name -eq 'OrganizerWorker.exe'} | ForEach-Object {[pscustomobject]@{pid=$_.ProcessId;creation=$_.CreationDate.ToUniversalTime().ToString('o');path=$_.ExecutablePath}})
$files=@(Get-FileHash (Join-Path $root 'worker.json'),(Join-Path $root 'current.json'),$cfg.ffmpeg_path -Algorithm SHA256 | Select-Object Path,Hash)
[pscustomobject]@{workers=$workers;files=$files;ffmpeg=$cfg.ffmpeg_path} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $dir 'before.json')
& nvidia-smi --query-gpu=index,name,uuid,pci.bus_id,driver_version,memory.total,memory.free,utilization.encoder --format=csv
$code=$LASTEXITCODE;Write-Output "GPU_INVENTORY_EXIT=$code";exit $code
