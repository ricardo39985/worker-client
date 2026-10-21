$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../scripts/worker-owned-files.ps1')
$root=Join-Path ([IO.Path]::GetTempPath()) ('ow-owned-script-'+[guid]::NewGuid().ToString('N'));[IO.Directory]::CreateDirectory($root)|Out-Null
try{
 $source=Join-Path $root 'source.ps1';$target=Join-Path $root 'start-worker.ps1';[IO.File]::WriteAllText($source,'reviewed-candidate');[IO.File]::WriteAllText($target,'pre-existing-operator-content')
 $rejected=$false;try{Install-WorkerOwnedScript $source $target}catch{$rejected=$true}
 if(-not $rejected -or [IO.File]::ReadAllText($target) -cne 'pre-existing-operator-content'){throw 'Installer overwrote unknown pre-existing script content'}
 [IO.File]::Delete($target);Install-WorkerOwnedScript $source $target
 if([IO.File]::ReadAllText($target) -cne 'reviewed-candidate'){throw 'New owned script not installed'}
 $before=(Get-Item $target).LastWriteTimeUtc;Install-WorkerOwnedScript $source $target;if((Get-Item $target).LastWriteTimeUtc -ne $before){throw 'Healthy existing script rewritten'}
 Write-Output 'OWNED_SCRIPT_PASS unknown existing retained, new-only create, healthy reuse'
}finally{Remove-Item -LiteralPath $root -Recurse -Force}
