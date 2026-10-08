#requires -Version 5.1
[CmdletBinding()]param([Parameter(Mandatory=$true)][string]$Build,[Parameter(Mandatory=$true)][string]$Destination,[Parameter(Mandatory=$true)][string]$Revision)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'setup-policy.ps1')
. (Join-Path $PSScriptRoot 'setup-bundle.ps1')
if($env:OS -ne 'Windows_NT' -or $Revision -notmatch '^[a-f0-9]{40}$'){throw 'Packaging requires native Windows and a reviewed source revision.'}
$source=Split-Path -Parent $PSScriptRoot
$inventory=(& ctest --test-dir $Build -C Release --show-only=json-v1 | Out-String) | ConvertFrom-Json
if($LASTEXITCODE -ne 0){throw 'Cannot enumerate native contracts.'}
foreach($name in 'worker_contracts','rendition_contracts','progress_contracts','inference_contracts','windows_contracts','setup_contracts','setup_progress_contracts','setup_bundle_contracts'){
 if($name -notin @($inventory.tests.name)){throw ('Mandatory native contract missing: '+$name)}
}
ctest --test-dir $Build -C Release --output-on-failure
if($LASTEXITCODE -ne 0){throw 'Native contracts failed; no package created.'}
if(Test-Path -LiteralPath $Destination){throw 'Package destination already exists; choose a fresh directory.'}
New-Item -ItemType Directory -Path $Destination | Out-Null
foreach($name in 'scripts','config'){Copy-Item -LiteralPath (Join-Path $source $name) -Destination $Destination -Recurse}
Copy-Item -LiteralPath (Join-Path $source 'setup.cmd') -Destination $Destination
$bundle=Join-Path $Destination 'bundle';New-Item -ItemType Directory -Path $bundle | Out-Null
$files=@();foreach($name in 'OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe'){
 $path=Join-Path $Build ('Release\'+$name);if(-not (Test-Path -LiteralPath $path)){throw ('Missing built binary: '+$name)}
 Copy-Item -LiteralPath $path -Destination $bundle
 $files+=@{path=$name;sha256=(Get-WorkerFileSha256 $path)}
}
Write-WorkerJson (Join-Path $bundle 'manifest.json') @{schema=1;architecture='x64';version='0.1.3';revision=$Revision;native_tests='passed';files=$files}
Test-WorkerBundle $bundle | Out-Null
Compress-Archive -Path (Join-Path $Destination '*') -DestinationPath ($Destination+'.zip')
Write-Host ('Bundle: '+$Destination+'.zip')
Write-Host ('SHA-256: '+(Get-WorkerFileSha256 ($Destination+'.zip')))
