# New-only installation of reviewed local scripts. Existing operator bytes are
# never overwritten or deleted, including the exact reviewed legacy launcher.
function Assert-WorkerPlainPath([string]$Path) {
    $p=[IO.Path]::GetFullPath($Path)
    if($p.StartsWith('\\',[StringComparison]::Ordinal)){throw 'Local per-machine path required'}
    while($p){
        try{if([IO.File]::GetAttributes($p) -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse installation path refused'}}
        catch{
            $e=$_.Exception;while($e.InnerException){$e=$e.InnerException}
            if($e -isnot [IO.FileNotFoundException] -and $e -isnot [IO.DirectoryNotFoundException]){throw}
        }
        $parent=[IO.Directory]::GetParent($p);if(-not $parent){break};$p=$parent.FullName
    }
}
function Install-WorkerOwnedScript([string]$Source,[string]$Destination) {
    Assert-WorkerPlainPath $Source;Assert-WorkerPlainPath $Destination
    if((Get-Item -LiteralPath $Source).Length -gt 262144){throw 'Reviewed script exceeds size bound'}
    if([IO.File]::Exists($Destination)){
        if((Get-Item -LiteralPath $Destination).Length -gt 262144){throw 'Unrecognized existing script retained'}
        $old=(Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash.ToLowerInvariant()
        $new=(Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash.ToLowerInvariant()
        if($old -ceq $new){return}
        $before=[IO.File]::ReadAllText($Destination).Replace("`r`n","`n")
        $candidate=[IO.File]::ReadAllText($Source).Replace("`r`n","`n")
        if($before -ceq $candidate){return}
        if([IO.Path]::GetFileName($Destination) -ceq 'start-worker.ps1' -and $old -in @('90e1a25c75f1f3d1941e8cf8e929efaca566428d9f6fbd67c17eb481f9318222','0a143de78e1df7f7ad64e344b35bddef763b8d35e76adb3e7e9eb57dbe416b9b')){return}
        throw 'Unknown pre-existing script retained; no overwrite or silent repair'
    }
    $temporary=$Destination+'.install-'+[guid]::NewGuid().ToString('N')+'.tmp';$created=$false
    try{
        $bytes=[IO.File]::ReadAllBytes($Source)
        $file=[IO.File]::Open($temporary,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$created=$true
        try{$file.Write($bytes,0,$bytes.Length);$file.Flush($true)}finally{$file.Dispose()}
        [IO.File]::Move($temporary,$Destination) # no overwrite if another writer appeared
    }finally{if($created -and [IO.File]::Exists($temporary)){[IO.File]::Delete($temporary)}}
}
