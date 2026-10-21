param([Parameter(Mandatory=$true)][string]$SourceRoot)
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
. (Join-Path $SourceRoot 'scripts/start-headless.ps1')
$base=Join-Path $env:TEMP ('ow-preexec-'+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($base)|Out-Null
$sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$everyone=New-Object Security.Principal.SecurityIdentifier('S-1-1-0')
$errors=New-Object 'Collections.Generic.List[string]'
function New-Case([string]$Name){
 $root=Join-Path $base $Name;$dir=Join-Path $root 'versions/good'
 [IO.Directory]::CreateDirectory($dir)|Out-Null
 $exe=Join-Path $dir 'OrganizerWorker.exe';[IO.File]::WriteAllText($exe,'safe-public-fixture-no-credentials')
 $hash=(Get-FileHash $exe).Hash.ToLowerInvariant()
 @{schema=1;directory=$dir;sha256=$hash}|ConvertTo-Json|Set-Content (Join-Path $root 'current.json')
 return @{Root=$root;Directory=$dir;Image=$exe;Hash=$hash;Marker=(Join-Path $root 'payload-ran')}
}
function Assert-NeverLaunch($Case){
 $launch={param($exe)[IO.File]::WriteAllText($Case.Marker,'UNSAFE PAYLOAD WOULD HAVE EXECUTED')}.GetNewClosure()
 try{Invoke-WorkerVerifiedImage -Root $Case.Root -ApprovedHashes @($Case.Hash) -OwnerSid $sid -Launch $launch|Out-Null}catch{}
 if(Test-Path $Case.Marker){throw 'Untrusted payload reached the execution boundary'}
}
try{
 $c=New-Case 'valid';$result=Invoke-WorkerVerifiedImage -Root $c.Root -ApprovedHashes @($c.Hash) -OwnerSid $sid -Launch ({param($exe)if([IO.Path]::GetFullPath($exe) -ine [IO.Path]::GetFullPath($c.Image)){throw 'Wrong image'};'APPROVED_IMAGE'}.GetNewClosure())
 if($result -cne 'APPROVED_IMAGE'){throw 'Valid installed owner image was not accepted'}
 foreach($name in @('writable-current','writable-ancestor','junction-ancestor','mutable-manifest','replacement-race')){
  try{
   $c=New-Case $name
   switch($name){
    'writable-current' {$p=Join-Path $c.Root 'current.json';$acl=Get-Acl $p;$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule($everyone,'Write','Allow')));Set-Acl $p $acl;Assert-NeverLaunch $c}
    'writable-ancestor' {$p=Join-Path $c.Root 'versions';$acl=Get-Acl $p;$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule($everyone,'DeleteSubdirectoriesAndFiles','Allow')));Set-Acl $p $acl;Assert-NeverLaunch $c}
    'junction-ancestor' {$external=Join-Path $base 'external';Move-Item (Join-Path $c.Root 'versions') $external;New-Item -ItemType Junction -Path (Join-Path $c.Root 'versions') -Target $external|Out-Null;Assert-NeverLaunch $c}
    'mutable-manifest' {[IO.File]::WriteAllText($c.Image,'foreign-payload');@{schema=1;directory=$c.Directory;sha256=(Get-FileHash $c.Image).Hash.ToLowerInvariant()}|ConvertTo-Json|Set-Content (Join-Path $c.Root 'current.json');Assert-NeverLaunch $c}
    'replacement-race' {
     Invoke-WorkerVerifiedImage -Root $c.Root -ApprovedHashes @($c.Hash) -OwnerSid $sid -Launch ({
      param($exe)
      $blocked=$false;try{[IO.File]::WriteAllText($exe,'foreign-swap-after-hash')}catch{$blocked=$true}
      if(-not $blocked){throw 'Validated image remains writable before execution'}
      $blocked=$false;try{Move-Item $c.Directory ($c.Directory+'-swapped') -ErrorAction Stop}catch{$blocked=$true}
      if(-not $blocked){throw 'Validated directory can be exchanged before execution'}
     }.GetNewClosure())|Out-Null
    }
   }
  }catch{$errors.Add($name+': '+$_.Exception.Message)}
 }
 if($errors.Count){throw ($errors -join ' | ')}
 'BOOTSTRAP_PREEXEC_PASS foreign ACL/junction/mutable manifest/race never reach payload; valid image accepted'
}finally{
 # All roots are this test's GUID-owned synthetic data, never a live owner root.
 $junction=Join-Path $base 'junction-ancestor/versions'
 if((Test-Path $junction) -and ((Get-Item -LiteralPath $junction).Attributes -band [IO.FileAttributes]::ReparsePoint)){[IO.Directory]::Delete($junction)}
 Remove-Item -LiteralPath $base -Recurse -Force
}
