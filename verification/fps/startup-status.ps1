$ProgressPreference='SilentlyContinue';$r=Join-Path $env:LOCALAPPDATA 'OrganizerWorker'
Get-Content (Join-Path $r 'current.json') -Raw
Get-Content (Join-Path $r 'worker-status.txt')
Get-Content (Join-Path $r 'worker.log') -Tail 5
