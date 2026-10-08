# Download/extraction ports are explicit so repair contracts run without Internet.
function Get-WorkerDependencyFiles([string]$Directory) {
    $p=New-WorkerProgress 'Enumerate dependency files' -Unit files
    $files=New-Object 'System.Collections.Generic.List[object]'
    try {
        Get-ChildItem -LiteralPath $Directory -Recurse -Force -ErrorAction Stop | ForEach-Object {
            if($_.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Generated dependency contains a link; refusing traversal.'}
            if(-not $_.PSIsContainer -and $_.Name -ne '.organizer-dependency.json'){
                $files.Add($_);Update-WorkerProgress $p -Completed $files.Count
            }
        }
        Complete-WorkerProgress $p -Detail ($files.Count.ToString()+' files found')
        return $files.ToArray()
    } catch {Complete-WorkerProgress $p -Failed -Detail $_.Exception.Message;throw}
}
function Save-WorkerDependencyManifest([string]$Directory,[string]$ArchiveSha256) {
    $prefix=[IO.Path]::GetFullPath($Directory).TrimEnd('\')+'\'
    $files=@(Get-WorkerDependencyFiles $Directory)
    if($files.Count -eq 0){throw 'Dependency extraction produced no files.'}
    $p=New-WorkerProgress 'Build dependency checksum manifest' -Total $files.Count -Unit files
    $records=New-Object 'System.Collections.Generic.List[object]'
    try {
        foreach($file in $files){
            $records.Add([pscustomobject]@{path=$file.FullName.Substring($prefix.Length);sha256=(Get-WorkerFileSha256 $file.FullName -Quiet)})
            Update-WorkerProgress $p -Completed $records.Count
        }
        Write-WorkerJson (Join-Path $Directory '.organizer-dependency.json') @{schema=1;archive_sha256=$ArchiveSha256;files=@($records.ToArray())}
        Complete-WorkerProgress $p
    } catch {Complete-WorkerProgress $p -Failed -Detail $_.Exception.Message;throw}
}
function Test-WorkerDependencyManifest([string]$Directory,[string]$ArchiveSha256) {
    $p=$null;$valid=$false;$failedFile='.organizer-dependency.json';$category='manifest'
    try {
        $path=Join-Path $Directory '.organizer-dependency.json'
        if(-not (Test-Path -LiteralPath $path)){return $false}
        $manifest=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        if($manifest.schema -ne 1 -or $manifest.archive_sha256 -ine $ArchiveSha256 -or @($manifest.files).Count -eq 0){return $false}
        # Enumeration also refuses links before hashing any cached files.
        $actual=@(Get-WorkerDependencyFiles $Directory)
        if($actual.Count -ne @($manifest.files).Count){$category='file-count';return $false}
        $p=New-WorkerProgress 'Verify cached dependency' -Total @($manifest.files).Count -Unit files
        $prefix=[IO.Path]::GetFullPath($Directory).TrimEnd('\')+'\';$done=0
        $seen=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
        foreach($file in $manifest.files){
            $failedFile=[string]$file.path;$category='path-or-missing-file'
            $full=[IO.Path]::GetFullPath((Join-Path $Directory $file.path))
            if(-not $full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or -not $seen.Add($full) -or -not (Test-Path -LiteralPath $full -PathType Leaf)){return $false}
            $category='checksum'
            if((Get-WorkerFileSha256 $full -Quiet) -ine $file.sha256){return $false}
            $done++;Update-WorkerProgress $p -Completed $done
        }
        $valid=$true;return $true
    } catch {$category='read-or-enumeration';return $false}
    finally {
        if(-not $valid){
            # Print only a sanitized relative name and a fixed category, never
            # exception bodies, file bytes or absolute credential paths.
            $nameForReport=if([IO.Path]::IsPathRooted($failedFile)){[IO.Path]::GetFileName($failedFile)}else{$failedFile}
            $safe=[regex]::Replace($nameForReport,'[^a-zA-Z0-9_./\\-]','?')
            if($safe.Length -gt 200){$safe=$safe.Substring(0,200)}
            Write-WorkerSetupLine ('[CACHE REJECTED] '+$category+' | '+$safe)
        }
        if($p){Complete-WorkerProgress $p -Failed:(-not $valid) -Detail $(if($valid){'Every cached file verified'}else{'Cache mismatch; fresh extraction required'})}
    }
}
function Move-WorkerDependencyContents([string]$Source,[string]$Destination) {
    $from=[IO.Path]::GetFullPath($Source).TrimEnd('\')+'\'
    $to=[IO.Path]::GetFullPath($Destination).TrimEnd('\')+'\'
    if($to.StartsWith($from,[StringComparison]::OrdinalIgnoreCase) -or $from.StartsWith($to,[StringComparison]::OrdinalIgnoreCase)){throw 'Dependency staging paths must be separate directories.'}
    if(-not [IO.Path]::GetPathRoot($from).Equals([IO.Path]::GetPathRoot($to),[StringComparison]::OrdinalIgnoreCase)){throw 'Dependency staging must remain on the same volume.'}
    $items=@(Get-ChildItem -LiteralPath $Source -Force)
    foreach($item in $items){if(Test-Path -LiteralPath (Join-Path $Destination $item.Name)){throw 'Dependency staging destination is not empty.'}}
    $p=New-WorkerProgress 'Move extracted dependency into staging' -Total $items.Count -Unit entries
    try {
        $done=0
        foreach($item in $items){Move-Item -LiteralPath $item.FullName -Destination $Destination;$done++;Update-WorkerProgress $p -Completed $done}
        Complete-WorkerProgress $p -Detail 'Same-volume move; no duplicate tree copy'
    } catch {Complete-WorkerProgress $p -Failed -Detail $_.Exception.Message;throw}
}
function Remove-WorkerGeneratedTree([string]$Directory) {
    # Callers pass generated dependency/build/probe trees, never identity or user media.
    if(-not (Test-Path -LiteralPath $Directory)){return}
    if((Get-Item -LiteralPath $Directory -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Generated directory is a link; refusing cleanup.'}
    $p=New-WorkerProgress 'Clean generated files' -Unit files
    $dirs=New-Object 'System.Collections.Generic.List[object]';$done=0
    try {
        Get-ChildItem -LiteralPath $Directory -Recurse -Force -ErrorAction Stop | ForEach-Object {
            if($_.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Generated tree contains a link; refusing cleanup.'}
            if($_.PSIsContainer){$dirs.Add($_)}else{Remove-Item -LiteralPath $_.FullName -Force;$done++;Update-WorkerProgress $p -Completed $done}
        }
        foreach($dir in ($dirs | Sort-Object {$_.FullName.Length} -Descending)){Remove-Item -LiteralPath $dir.FullName -Force}
        Remove-Item -LiteralPath $Directory -Force
        Complete-WorkerProgress $p -Detail ($done.ToString()+' generated files removed')
    } catch {Complete-WorkerProgress $p -Failed -Detail $_.Exception.Message;throw}
}
function Expand-WorkerDependency([string]$Directory,[string]$ArchiveSha256,[scriptblock]$Extract) {
    if(Test-WorkerDependencyManifest $Directory $ArchiveSha256){Write-WorkerSetupLine '[CACHE] Verified extracted dependency reused.';return}
    $parent=Split-Path -Parent $Directory
    $stage=Join-Path $parent ('extract-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    try {
        & $Extract $stage | Out-Null
        Save-WorkerDependencyManifest $stage $ArchiveSha256
        if(Test-Path -LiteralPath $Directory){Write-WorkerSetupLine '[REPAIR] Replacing damaged generated dependency files.';Remove-WorkerGeneratedTree $Directory}
        Move-Item -LiteralPath $stage -Destination $Directory
    } finally {if(Test-Path -LiteralPath $stage){Remove-WorkerGeneratedTree $stage}}
}
