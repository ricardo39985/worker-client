$r=Join-Path $env:LOCALAPPDATA 'Temp\ow-installer-swap-20261018'
Get-Content (Join-Path $r 'console-capture-result.txt')
if(Test-Path (Join-Path $r 'console-buffer.txt')){Get-Content (Join-Path $r 'console-buffer.txt')}
