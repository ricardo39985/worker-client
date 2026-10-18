$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$owned=@(Get-CimInstance Win32_Process -Filter "Name='ffmpeg.exe'" | Where-Object {$_.ExecutablePath -ieq 'C:\Users\danny\ffmpeg.exe' -and $_.CommandLine -like '*Temp\ow-fps-20261018\after-real3-nvenc\optimized.mp4*'})
foreach($child in $owned){$p=Get-Process -Id $child.ProcessId;$null=$p.Handle;Stop-Process -Id $p.Id;if(-not $p.WaitForExit(10000)){throw 'Owned measurement child did not drain'};Write-Output ('OWNED_MEASUREMENT_STOPPED_PID='+$p.Id)}
Write-Output ('OWNED_CHILDREN_DRAINED='+$owned.Count)
