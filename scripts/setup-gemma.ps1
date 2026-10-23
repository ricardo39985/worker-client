#requires -Version 5.1
[CmdletBinding()]
param([string]$Root=(Join-Path $env:LOCALAPPDATA 'OrganizerWorker'),
      [Parameter(Mandatory=$true)][string]$Python,
      [Parameter(Mandatory=$true)][string]$Ffmpeg,
      [string]$Ffprobe='',
      [switch]$AcceptRuntimeLicenses)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
if(-not $AcceptRuntimeLicenses){throw 'Review config/gemma.lock.json: llama.cpp MIT, EmbeddingGemma 2 Apache-2.0 and optional FFprobe GPL. Pass -AcceptRuntimeLicenses to provision this optional runtime.'}
$lock=Get-Content (Join-Path $PSScriptRoot '..\config\gemma.lock.json') -Raw|ConvertFrom-Json
foreach($path in @($Python,$Ffmpeg)){if(-not [IO.Path]::IsPathRooted($path) -or -not (Test-Path -LiteralPath $path -PathType Leaf)){throw 'Existing absolute Python/FFmpeg paths required'}}
$destination=Join-Path $Root 'gemma\b11475-q8-segments-v2'
$models=Join-Path $destination 'models';$runtime=Join-Path $destination 'runtime'
$null=New-Item -ItemType Directory -Force $models,$runtime
function Get-PinnedFile([string]$Url,[string]$Hash,[string]$Target){
    if((Test-Path -LiteralPath $Target) -and (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant() -eq $Hash){return}
    $pending=$Target+'.download'
    Write-Output ('Downloading pinned dependency: '+[IO.Path]::GetFileName($Target))
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $pending -TimeoutSec 1800
    if((Get-FileHash -LiteralPath $pending -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Hash){throw 'Pinned dependency checksum differs'}
    Move-Item -LiteralPath $pending -Destination $Target -Force
}
$archive=Join-Path $destination 'llama-b11475-bin-win-cpu-x64.zip'
Get-PinnedFile $lock.runtime.url $lock.runtime.sha256 $archive
Add-Type -AssemblyName System.IO.Compression.FileSystem
if(-not $Ffprobe){
    $ffarchive=Join-Path $destination 'ffmpeg-upstream.zip'
    Get-PinnedFile $lock.ffprobe.url $lock.ffprobe.sha256 $ffarchive
    $ffzip=[IO.Compression.ZipFile]::OpenRead($ffarchive)
    try {
        $entry=$ffzip.GetEntry($lock.ffprobe.entry)
        if(-not $entry){throw 'Pinned FFmpeg archive lacks FFprobe'}
        $Ffprobe=Join-Path $destination 'ffprobe.exe'
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$Ffprobe,$true)
    } finally {$ffzip.Dispose()}
}
if(-not [IO.Path]::IsPathRooted($Ffprobe) -or -not (Test-Path -LiteralPath $Ffprobe -PathType Leaf)){throw 'Absolute FFprobe path required'}
$zip=[IO.Compression.ZipFile]::OpenRead($archive)
try {
    foreach($entry in $zip.Entries){
        if(-not $entry.Name){continue}
        if($entry.FullName -ne $entry.Name -or $entry.Name -match '[:\\/]'){throw 'Unexpected runtime archive path'}
        $target=Join-Path $runtime $entry.Name
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$true)
    }
} finally {$zip.Dispose()}
$files=@{};Get-ChildItem -LiteralPath $runtime -File|ForEach-Object {$files[$_.Name]=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
if(-not $files.ContainsKey('llama-server.exe')){throw 'Pinned archive lacks llama-server.exe'}
$files|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $destination 'runtime-files.json') -Encoding UTF8
foreach($property in $lock.model.files.PSObject.Properties){
    $url='https://huggingface.co/'+$lock.model.repository+'/resolve/'+$lock.model.revision+'/'+$property.Name
    Get-PinnedFile $url $property.Value (Join-Path $models $property.Name)
}
$engineHash=(Get-FileHash (Join-Path $PSScriptRoot 'gemma_inference.py')).Hash.ToLowerInvariant()
$engine=Join-Path $destination ('gemma_inference-'+$engineHash.Substring(0,16)+'.py')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'gemma_inference.py') -Destination $engine -Force
$probe=Join-Path $destination 'probe.json'
$arguments=@('-I',('"'+$engine+'"'),'--runtime-root',('"'+$destination+'"'),'--output',('"'+$probe+'"'),'probe','--ffmpeg',('"'+$Ffmpeg+'"'),'--ffprobe',('"'+$Ffprobe+'"'))
$process=Start-Process -FilePath $Python -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $destination 'setup-probe.stdout.log') -RedirectStandardError (Join-Path $destination 'setup-probe.stderr.log')
if($process.ExitCode){throw 'Real local Gemma probe failed; capability remains disabled'}
$evidence=Get-Content $probe -Raw|ConvertFrom-Json
if($evidence.profile -ne $lock.profile -or -not $evidence.image_vectors_differ -or $evidence.dimensions -ne 768){throw 'Gemma probe evidence differs'}
$config=@{schema=1;enabled=$true;advertise=$false;profile=$lock.profile;runtime_root=$destination;python=$Python;
    python_sha256=(Get-FileHash $Python).Hash.ToLowerInvariant();script=$engine;
    script_sha256=(Get-FileHash $engine).Hash.ToLowerInvariant();ffprobe=$Ffprobe;
    ffprobe_sha256=(Get-FileHash $Ffprobe).Hash.ToLowerInvariant()}
$pending=Join-Path $Root 'gemma.json.pending'
$config|ConvertTo-Json|Set-Content $pending -Encoding UTF8
Move-Item $pending (Join-Path $Root 'gemma.json') -Force
Write-Output 'Gemma local text/image/video/audio probes passed; runtime provisioned. Existing worker has not been stopped or restarted.'
