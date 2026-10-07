# Download/extraction ports are explicit so repair contracts run without Internet.
function Save-WorkerDependencyManifest([string]$Directory,[string]$ArchiveSha256) {
    $prefix=[IO.Path]::GetFullPath($Directory).TrimEnd('\')+'\'
    $files=@(Get-ChildItem -LiteralPath $Directory -Recurse -File | Where-Object {$_.Name -ne '.organizer-dependency.json'} | ForEach-Object {
        [pscustomobject]@{path=$_.FullName.Substring($prefix.Length);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    })
    if ($files.Count -eq 0) { throw 'Dependency extraction produced no files.' }
    Write-WorkerJson (Join-Path $Directory '.organizer-dependency.json') @{schema=1;archive_sha256=$ArchiveSha256;files=$files}
}
function Test-WorkerDependencyManifest([string]$Directory,[string]$ArchiveSha256) {
    try {
        $path=Join-Path $Directory '.organizer-dependency.json'
        if (-not (Test-Path -LiteralPath $path)) { return $false }
        $manifest=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        if ($manifest.schema -ne 1 -or $manifest.archive_sha256 -ine $ArchiveSha256 -or @($manifest.files).Count -eq 0) { return $false }
        $prefix=[IO.Path]::GetFullPath($Directory).TrimEnd('\')+'\'
        foreach ($file in $manifest.files) {
            $full=[IO.Path]::GetFullPath((Join-Path $Directory $file.path))
            if (-not $full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $full -PathType Leaf)) { return $false }
            if ((Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash -ine $file.sha256) { return $false }
        }
        return (@(Get-ChildItem -LiteralPath $Directory -Recurse -File | Where-Object {$_.Name -ne '.organizer-dependency.json'}).Count -eq @($manifest.files).Count)
    } catch { return $false }
}
function Expand-WorkerDependency([string]$Directory,[string]$ArchiveSha256,[scriptblock]$Extract) {
    if (Test-WorkerDependencyManifest $Directory $ArchiveSha256) { return }
    $parent=Split-Path -Parent $Directory
    $stage=Join-Path $parent ('extract-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    try {
        & $Extract $stage | Out-Null
        Save-WorkerDependencyManifest $stage $ArchiveSha256
        if (Test-Path -LiteralPath $Directory) {
            Write-Host '[REPAIR] Replacing damaged generated dependency files.'
            Remove-Item -LiteralPath $Directory -Recurse -Force
        }
        Move-Item -LiteralPath $stage -Destination $Directory
    } finally { if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force } }
}
