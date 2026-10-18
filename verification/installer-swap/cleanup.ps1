$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$stage=Join-Path $env:LOCALAPPDATA 'Temp\ow-installer-swap-20261018'
if(Test-Path -LiteralPath $stage) {
 $item=Get-Item -LiteralPath $stage
 if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Refusing staging reparse point'}
 Remove-Item -LiteralPath $stage -Recurse -Force
}
Write-Output ('STAGING_REMOVED='+(-not [IO.Directory]::Exists($stage)))
if([IO.Directory]::Exists($stage)){throw 'Staging still exists'}
