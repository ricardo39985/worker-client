#requires -Version 5.1
# Offline filesystem contracts; the synthetic .exe is never executed.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../scripts/setup-policy.ps1')
. (Join-Path $PSScriptRoot '../scripts/setup-converter.ps1')
# Any accidental interactive fallback must fail immediately in this offline suite.
function Read-Host {throw 'Unexpected input request in offline converter contracts'}
$script:passed=0;$script:failed=0
function Assert($Value,[string]$Message='Assertion failed'){if(-not $Value){throw $Message}}
function Rejects([scriptblock]$Body,[string]$Reason=''){
    $caught=$false
    try{& $Body | Out-Null}catch{$caught=$true;if($Reason){Assert ($_.Exception.Message -like ('*'+$Reason+'*')) $_.Exception.Message}}
    Assert $caught 'Expected explicit failure'
}
function Case([string]$Name,[scriptblock]$Body){
    $caseRoot=Join-Path $root ([guid]::NewGuid().ToString('N').Substring(0,16));New-Item -ItemType Directory $caseRoot | Out-Null
    Write-Host "START $Name"
    try{& $Body;Write-Host "PASS $Name";$script:passed++}catch{Write-Host "FAIL $Name : $($_.Exception.Message)";$script:failed++}
}
# Keep synthetic .part paths below legacy PowerShell MAX_PATH in deep isolated roots.
$root=Join-Path ([IO.Path]::GetTempPath()) ('ow-c-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $root | Out-Null
$payload=Join-Path $root 'synthetic.exe';[IO.File]::WriteAllText($payload,'synthetic, never executed')
$sha=(Get-WorkerFileSha256 $payload -Quiet).ToLowerInvariant()
function Entry {
    return [pscustomobject]@{version='reviewed';format='exe';distribution='operator-retained';sha256=$sha;bytes=(Get-Item $payload).Length;url=('https://artifacts.example.test/'+$sha+'/ffmpeg.exe');license='GPL reviewed';license_url='https://ffmpeg.org/legal.html';redistribution_url='https://artifacts.example.test/source-and-notices.html'}
}
try {
 Case 'a fresh machine downloads pinned bytes after consent and records acceptance' {
    $entry=Entry;$state=@{consent=0;downloads=0}
    $result=Install-WorkerLockedConverter -Dependency $entry -Root $caseRoot -Consent {$state.consent++;$true} -Download {param($url,$partial)Assert ($url -eq $entry.url);$state.downloads++;Copy-Item $payload $partial}
    Assert ($result.sha256 -eq $sha -and (Get-WorkerFileSha256 $result.path -Quiet) -eq $sha)
    Assert ($state.consent -eq 1 -and $state.downloads -eq 1)
    Assert (Test-Path (Join-Path $caseRoot ('ffmpeg-license-'+$sha+'.json')))
 }
 Case 'corrupt transfers retry the supplied transport and only verified bytes publish' {
    $entry=Entry;$state=@{downloads=0}
    $result=Install-WorkerLockedConverter -Dependency $entry -Root $caseRoot -AcceptLicense -Download {
        param($url,$partial)
        Assert ($url -ceq $entry.url) 'Retry changed the reviewed URL'
        Assert ($partial.StartsWith($caseRoot) -and $partial.EndsWith('.part')) 'Transport did not receive a private partial path'
        $state.downloads++
        Assert ($state.downloads -le 3) 'Unbounded transport retries'
        if($state.downloads -lt 3){[IO.File]::WriteAllText($partial,('x' * [int]$entry.bytes))}
        else{Copy-Item $payload $partial}
    }
    Assert ($state.downloads -eq 3)
    Assert ((Get-WorkerFileSha256 $result.path -Quiet) -ceq $sha)
    Assert (@(Get-ChildItem $caseRoot -Filter '*.part' -Recurse).Count -eq 0)
    $receipt=Get-Content (Join-Path $caseRoot ('ffmpeg-license-'+$sha+'.json')) -Raw | ConvertFrom-Json
    Assert ($receipt.artifact_sha256 -ceq $sha)
 }
 Case 'a verified retained converter is reused without another transfer or prompt' {
    $first=Install-WorkerLockedConverter -Dependency (Entry) -Root $caseRoot -AcceptLicense -Download {param($url,$partial)Copy-Item $payload $partial}
    $next=Install-WorkerLockedConverter -Dependency (Entry) -Root $caseRoot -Download {throw 'unexpected transfer'} -Consent {throw 'unexpected prompt'}
    Assert ($first.path -eq $next.path)
 }
 Case 'a tampered installed converter is replaced with verified bytes' {
    $entry=Entry;$first=Install-WorkerLockedConverter -Dependency $entry -Root $caseRoot -AcceptLicense -Download {param($url,$partial)Copy-Item $payload $partial}
    [IO.File]::WriteAllText($first.path,'tampered')
    $result=Install-WorkerLockedConverter -Dependency $entry -Root $caseRoot -Download {param($url,$partial)Copy-Item $payload $partial}
    Assert ((Get-WorkerFileSha256 $result.path -Quiet) -eq $sha)
 }
 Case 'same-size wrong bytes cannot publish converter or license receipt' {
    $entry=Entry
    Rejects {Install-WorkerLockedConverter -Dependency $entry -Root $caseRoot -AcceptLicense -Download {param($url,$partial)[IO.File]::WriteAllText($partial,('x' * [int]$entry.bytes))}} 'checksum'
    Assert (@(Get-ChildItem $caseRoot -Filter '*.part' -Recurse).Count -eq 0)
    Assert (-not (Test-Path (Join-Path $caseRoot ('ffmpeg-license-'+$sha+'.json'))))
    Assert (@(Get-ChildItem $caseRoot -Filter 'ffmpeg.exe' -Recurse).Count -eq 0)
 }
 Case 'truncated download is rejected and temporary bytes removed' {
    Rejects {Install-WorkerLockedConverter -Dependency (Entry) -Root $caseRoot -AcceptLicense -Download {param($url,$partial)[IO.File]::WriteAllText($partial,'short')}} 'byte count'
    Assert (@(Get-ChildItem $caseRoot -Filter '*.part' -Recurse).Count -eq 0)
 }
 Case 'unpublished artifact fails clearly before consent or network' {
    $entry=Entry;$entry.url=$null;$entry.redistribution_url=$null
    Rejects {Install-WorkerLockedConverter -Dependency $entry -Root $caseRoot -Consent {throw 'unexpected consent'} -Download {throw 'unexpected network'}} 'publication pending'
 }
 Case 'NoConversion bypasses unpublished dependency and clears converter selection' {
    $entry=Entry;$entry.url=$null
    $result=Install-WorkerLockedConverter -Dependency $entry -Root $caseRoot -NoConversion -Download {throw 'unexpected network'}
    Assert ($result.path -eq '' -and $result.sha256 -eq '')
 }
 Case 'an already approved installed converter survives a pending publication' {
    $entry=Entry;$entry.url=$null;$entry.redistribution_url=$null
    $result=Install-WorkerLockedConverter -Dependency $entry -Root $caseRoot -StoredPath $payload -StoredSha256 $sha -Download {throw 'unexpected network'}
    Assert ($result.path -eq $payload -and $result.sha256 -eq $sha)
 }
 Case 'a damaged stored converter cannot bypass pending publication' {
    $entry=Entry;$entry.url=$null;$entry.redistribution_url=$null
    Rejects {Install-WorkerLockedConverter -Dependency $entry -Root $caseRoot -StoredPath $payload -StoredSha256 ('a'*64)} 'publication pending'
 }
 Case 'noninteractive installation requires explicit license consent' {
    Rejects {Install-WorkerLockedConverter -Dependency (Entry) -Root $caseRoot -NonInteractive -Download {throw 'unexpected network'}} 'AcceptConversionLicense'
 }
 Case 'declining consent performs no download and creates no receipt' {
    Rejects {Install-WorkerLockedConverter -Dependency (Entry) -Root $caseRoot -Consent {$false} -Download {throw 'unexpected network'}} 'cancelled'
    Assert (@(Get-ChildItem $caseRoot -Recurse).Count -eq 0)
 }
 Case 'a receipt for another digest does not authorize this dependency' {
    Write-WorkerJson (Join-Path $caseRoot ('ffmpeg-license-'+$sha+'.json')) @{artifact_sha256=('a'*64)}
    Rejects {Install-WorkerLockedConverter -Dependency (Entry) -Root $caseRoot -NonInteractive -Download {throw 'unexpected network'}} 'AcceptConversionLicense'
 }
 Case 'network failure retains unrelated identity and cannot publish success' {
    $identity=Join-Path $caseRoot 'credential.dpapi';[IO.File]::WriteAllText($identity,'retained')
    $state=@{downloads=0}
    Rejects {Install-WorkerLockedConverter -Dependency (Entry) -Root $caseRoot -AcceptLicense -Download {$state.downloads++;throw 'HTTP 404'}} 'HTTP 404'
    Assert ($state.downloads -eq 3) 'Transport exhaustion must stop after three attempts'
    Assert (@(Get-ChildItem $caseRoot -Filter '*.part' -Recurse).Count -eq 0)
    Assert ([IO.File]::ReadAllText($identity) -eq 'retained')
    Assert (-not (Test-Path (Join-Path $caseRoot ('ffmpeg-license-'+$sha+'.json'))))
 }
 Case 'unsafe expiring and unpinned endpoints fail before fetching' {
    foreach($url in @('http://host.test/file',('https://host.test/latest/'+$sha+'/ffmpeg.exe'),('https://host.test/%6catest/'+$sha+'/ffmpeg.exe'),('https://github.com/BtbN/FFmpeg-Builds/autobuild-foo/'+$sha+'/ffmpeg.exe'),'https://host.test/ffmpeg.exe',('https://host.test/'+$sha+'/ffmpeg.exe?secret=x'))){
        $entry=Entry;$entry.url=$url
        Rejects {Install-WorkerLockedConverter -Dependency $entry -Root $caseRoot -AcceptLicense -Download {throw 'must not download'}} 'lock'
    }
 }
 Case 'invalid executable metadata and missing checksum fail closed' {
    foreach($field in @('sha256','format','bytes','redistribution_url')){
        $entry=Entry;$entry.$field=if($field -eq 'bytes'){0}else{''}
        Rejects {Install-WorkerLockedConverter -Dependency $entry -Root $caseRoot -AcceptLicense} 'lock'
    }
 }
 Case 'forced repair cannot switch a supplied converter silently' {
    $entry=Entry;$entry.url=$null;$entry.redistribution_url=$null
    Rejects {Install-WorkerLockedConverter -Dependency $entry -Root $caseRoot -StoredPath $payload -StoredSha256 $sha -Force} 'publication pending'
    Assert ((Get-WorkerFileSha256 $payload -Quiet) -eq $sha)
 }
} finally {Remove-Item -LiteralPath $root -Recurse -Force}
Write-Host "$script:passed converter contracts passed; $script:failed failed"
if($script:failed){exit 1}
