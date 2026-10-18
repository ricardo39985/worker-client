$ProgressPreference='SilentlyContinue'
$r=Join-Path $env:LOCALAPPDATA 'OrganizerWorker'
Get-ChildItem $r -Directory | Select-Object Name
Get-ChildItem (Join-Path $r 'tools') -Recurse -File -ErrorAction SilentlyContinue | Where-Object {$_.Name -match '^ff(mpeg|probe).exe$'} | Select-Object FullName
$c=Get-Content (Join-Path $r 'worker.json') -Raw | ConvertFrom-Json
$c.PSObject.Properties | Where-Object {$_.Name -match 'ffmpeg|converter'} | ForEach-Object {Write-Output ($_.Name+'='+$_.Value)}
