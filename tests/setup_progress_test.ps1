#requires -Version 5.1
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot '..\scripts\setup-policy.ps1')
. (Join-Path $PSScriptRoot '..\scripts\setup-dependencies.ps1')
$passed=0;$failed=0
function Assert($Value,[string]$Message='Assertion failed') { if (-not $Value) { throw $Message } }
function Rejects([scriptblock]$Body) { $caught=$false;try { & $Body | Out-Null } catch { $caught=$true };Assert $caught 'Expected failure' }
function Case([string]$Name,[scriptblock]$Body) { try { & $Body;Write-Host "PASS $Name";$script:passed++ } catch {Write-Host "FAIL $Name : $($_.Exception.Message)";$script:failed++} }
$root=Join-Path ([IO.Path]::GetTempPath()) ('ow-progress-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
try {
 Case 'unknown total never acquires a percentage just because time passes' {
  $time=@{now=0};$events=New-Object 'System.Collections.Generic.List[object]'
  $p=New-WorkerProgress 'Unknown operation' -Clock {$time.now} -Emit {param($e)$events.Add($e)}
  $time.now=5000;Update-WorkerProgress $p
  Assert ($events[-1].percent -eq $null)
  Assert ($events[-1].completed -eq 0)
  Assert ($events[-1].elapsed_seconds -eq 5)
 }
 Case 'measured work gives a percentage and successful completion ends the stage' {
  $time=@{now=0};$events=New-Object 'System.Collections.Generic.List[object]'
  $p=New-WorkerProgress 'Files' -Total 10 -Unit files -Clock {$time.now} -Emit {param($e)$events.Add($e)}
  $time.now=3000;Update-WorkerProgress $p -Completed 4
  Assert ($events[-1].percent -eq 40)
  Complete-WorkerProgress $p
  Assert ($events[-1].status -eq 'done' -and $events[-1].percent -eq 100)
  $n=$events.Count;$time.now=10000;Update-WorkerProgress $p
  Assert ($events.Count -eq $n) 'Finished stages must not keep printing heartbeats'
 }
 Case 'heartbeat reports quiet activity without pretending work advanced' {
  $time=@{now=0};$events=New-Object 'System.Collections.Generic.List[object]'
  $p=New-WorkerProgress 'Waiting' -Total 100 -QuietSeconds 5 -Clock {$time.now} -Emit {param($e)$events.Add($e)}
  $time.now=6000;Update-WorkerProgress $p
  Assert ($events[-1].status -eq 'quiet' -and $events[-1].completed -eq 0)
  $time.now=8000;Update-WorkerProgress $p -Completed 2
  Assert ($events[-1].status -eq 'progress' -and $events[-1].quiet_seconds -eq 0)
 }
 Case 'failure stays a failure and does not display completed work' {
  $events=New-Object 'System.Collections.Generic.List[object]'
  $p=New-WorkerProgress 'Failed transfer' -Total 100 -Emit {param($e)$events.Add($e)}
  Complete-WorkerProgress $p -Failed -Detail 'synthetic failure'
  Assert ($events[-1].status -eq 'failed' -and $events[-1].percent -eq 0)
 }
 Case 'URLs and credential values never reach a progress log' {
  $safe=Protect-WorkerSetupOutput 'url=https://storage.test/object?X-Amz-Signature=private-signature token="private-token" Authorization: Bearer private-bearer'
  Assert (-not $safe.Contains('private-signature'))
  Assert (-not $safe.Contains('private-token'))
  Assert (-not $safe.Contains('private-bearer'))
 }
 Case 'streaming checksum retains the actual SHA256 contract' {
  $path=Join-Path $root 'hash.bin';[IO.File]::WriteAllText($path,'abc',[Text.UTF8Encoding]::new($false))
  Assert ((Get-WorkerFileSha256 $path -Quiet) -eq 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')
 }
 Case 'stream transfer counts real bytes and rejects truncated content' {
  $events=New-Object 'System.Collections.Generic.List[object]'
  $p=New-WorkerProgress 'Synthetic bytes' -Emit {param($e)$events.Add($e)}
  $stream=[IO.MemoryStream]::new([Text.Encoding]::UTF8.GetBytes('abcdef'))
  try {Receive-WorkerStream $stream (Join-Path $root 'received.bin') 6 $p} finally {$stream.Dispose()}
  Assert ([IO.File]::ReadAllText((Join-Path $root 'received.bin')) -eq 'abcdef')
  Assert ($p.Completed -eq 6 -and $p.Total -eq 6)
  $stream=[IO.MemoryStream]::new([Text.Encoding]::UTF8.GetBytes('abc'))
  try {Rejects {Receive-WorkerStream $stream (Join-Path $root 'truncated.bin') 9 (New-WorkerProgress 'Truncated')}} finally {$stream.Dispose()}
 }
 Case 'staging move preserves dependency contents and unrelated user state' {
  $source=Join-Path $root 'unpacked';$stage=Join-Path $root 'stage'
  New-Item -ItemType Directory -Path $source,$stage | Out-Null
  New-Item -ItemType Directory -Path (Join-Path $source 'headers') | Out-Null
  [IO.File]::WriteAllText((Join-Path $source 'headers\library.hpp'),'trusted bytes')
  $identity=Join-Path $root 'identity.dpapi';[IO.File]::WriteAllText($identity,'retained identity')
  Move-WorkerDependencyContents $source $stage
  Assert ([IO.File]::ReadAllText((Join-Path $stage 'headers\library.hpp')) -eq 'trusted bytes')
  Assert ([IO.File]::ReadAllText($identity) -eq 'retained identity')
  Save-WorkerDependencyManifest $stage ('a'*64)
  Assert (Test-WorkerDependencyManifest $stage ('a'*64))
 }
 Case 'dependency staging collision fails without overwriting either tree' {
  $from=Join-Path $root 'collision-source';$to=Join-Path $root 'collision-destination'
  New-Item -ItemType Directory -Path $from,$to | Out-Null
  [IO.File]::WriteAllText((Join-Path $from 'same.txt'),'incoming')
  [IO.File]::WriteAllText((Join-Path $to 'same.txt'),'existing')
  Rejects {Move-WorkerDependencyContents $from $to}
  Assert ([IO.File]::ReadAllText((Join-Path $from 'same.txt')) -eq 'incoming')
  Assert ([IO.File]::ReadAllText((Join-Path $to 'same.txt')) -eq 'existing')
 }
 Case 'download bounds reject oversized and declared-empty content' {
  $stream=[IO.MemoryStream]::new([Text.Encoding]::UTF8.GetBytes('abcdef'))
  try {Rejects {Receive-WorkerStream $stream (Join-Path $root 'oversized.bin') 6 (New-WorkerProgress 'Oversized') -MaximumBytes 3}} finally {$stream.Dispose()}
  $stream=[IO.MemoryStream]::new([Text.Encoding]::UTF8.GetBytes('abc'))
  try {Rejects {Receive-WorkerStream $stream (Join-Path $root 'declared-empty.bin') 0 (New-WorkerProgress 'Declared empty')}} finally {$stream.Dispose()}
 }
 Case 'owned ZIP extractor preserves real nested dependency files' {
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  $from=Join-Path $root 'zip-source';$to=Join-Path $root 'zip-output';$zip=Join-Path $root 'fixture.zip'
  New-Item -ItemType Directory -Path (Join-Path $from 'nested') -Force | Out-Null
  [IO.File]::WriteAllText((Join-Path $from 'nested\fixture.txt'),'synthetic archive bytes')
  [IO.Compression.ZipFile]::CreateFromDirectory($from,$zip)
  Invoke-WorkerNativeProgress (Join-Path $PSHOME 'powershell.exe') @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot '..\scripts\setup-extract-zip.ps1'),'-Archive',$zip,'-Destination',$to) -Name 'Synthetic ZIP' -TimeoutSeconds 20
  Assert ([IO.File]::ReadAllText((Join-Path $to 'nested\fixture.txt')) -eq 'synthetic archive bytes')
 }
 Case 'real native stdout and stderr are delivered while the child is still running' {
  $child=Join-Path $root 'stream.ps1'
  [IO.File]::WriteAllText($child,'[Console]::Out.WriteLine("first:"+$PID);[Console]::Error.WriteLine("stderr");Start-Sleep -Seconds 2')
  $seen=@{early=$false;error=$false}
  $ps=Join-Path $PSHOME 'powershell.exe'
  Invoke-WorkerNativeProgress $ps @('-NoProfile','-ExecutionPolicy','Bypass','-File',$child) -Name 'Synthetic child' -TimeoutSeconds 10 -OnLine {
   param($line,$isError)
   if($line.StartsWith('first:')){$childId=[int]$line.Substring(6);$seen.early=($null -ne (Get-Process -Id $childId -ErrorAction SilentlyContinue))}
   if($isError -and $line -eq 'stderr'){$seen.error=$true}
  }
  Assert ($seen.early -and $seen.error)
 }
 Case 'native arguments retain spaces quotes empty values and trailing slashes' {
  # A real .NET executable exercises Windows argv parsing without PowerShell.exe's own -File parser.
  $child=Join-Path $root 'arguments.exe'
  Add-Type -TypeDefinition 'using System; using System.Text; class ArgumentFixture { static void Main(string[] args) { foreach(string a in args) Console.WriteLine(a.Length+"|"+Convert.ToBase64String(Encoding.UTF8.GetBytes(a))); } }' -OutputAssembly $child -OutputType ConsoleApplication
  $expected=@('space here','embedded"quote','C:\ends\','')
  $result=Invoke-WorkerNativeProgress $child $expected -Name 'Argument fixture' -CaptureOutput
  $actual=@($result.TrimEnd([char[]]"`r`n").Split([string[]]@([Environment]::NewLine),[StringSplitOptions]::None) | ForEach-Object {
   $parts=$_.Split('|');[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($parts[1]))
  })
  Assert ($actual.Count -eq $expected.Count)
  for($i=0;$i -lt $expected.Count;$i++){Assert ($actual[$i] -ceq $expected[$i])}
 }
 Case 'a nonzero native exit cannot report a successful stage' {
  $child=Join-Path $root 'failure.ps1';[IO.File]::WriteAllText($child,'[Console]::Error.WriteLine("synthetic defect");exit 7')
  Rejects {Invoke-WorkerNativeProgress (Join-Path $PSHOME 'powershell.exe') @('-NoProfile','-ExecutionPolicy','Bypass','-File',$child) -Name 'Failing child'}
 }
 Case 'native timeout stops the owned child and remains a failure' {
  $child=Join-Path $root 'timeout.ps1';[IO.File]::WriteAllText($child,'[Console]::Out.WriteLine("pid:"+$PID);Start-Sleep -Seconds 60')
  $seen=@{id=0}
  Rejects {Invoke-WorkerNativeProgress (Join-Path $PSHOME 'powershell.exe') @('-NoProfile','-ExecutionPolicy','Bypass','-File',$child) -Name 'Timed out child' -TimeoutSeconds 3 -OnLine {param($line,$isError)if($line.StartsWith('pid:')){$seen.id=[int]$line.Substring(4)}}}
  Assert ($seen.id -gt 0)
  Assert ($null -eq (Get-Process -Id $seen.id -ErrorAction SilentlyContinue))
 }
} finally {Remove-Item -LiteralPath $root -Recurse -Force}
Write-Host "$passed progress contracts passed; $failed failed"
if($failed){exit 1}
