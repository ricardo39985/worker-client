#requires -Version 5.1
[CmdletBinding()]
param(
    [switch]$CheckOnly,
    [switch]$InstallMissing,
    [switch]$AcceptToolLicenses,
    [switch]$AcceptConversionLicense,
    [switch]$AcceptStorageHosts,
    [switch]$NoConversion,
    [string]$Server,
    [string[]]$StorageHosts,
    [string]$FfmpegPath,
    [string]$FfmpegSha256,
    [switch]$StartAtLogon,
    [switch]$Launch,
    [switch]$NoStartup,
    [switch]$NoLaunch,
    [switch]$NonInteractive,
    [switch]$RePair,
    [switch]$ElevatedBuildToolsOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
. (Join-Path $PSScriptRoot 'setup-policy.ps1')
. (Join-Path $PSScriptRoot 'setup-dependencies.ps1')
. (Join-Path $PSScriptRoot 'setup-startup.ps1')
. (Join-Path $PSScriptRoot 'setup-identity.ps1')
$Source = Split-Path -Parent $PSScriptRoot
$Checks = New-Object System.Collections.Generic.List[object]
$Root = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'OrganizerWorker'
$ReportPath = Join-Path $Source 'verification\windows-setup.json'
$script:Compiler = $null
$script:CMake = $null
$script:Sdk = $null
$script:InstallGuard = $null
$script:ExplicitFfmpeg=$PSBoundParameters.ContainsKey('FfmpegPath')
$script:WholeCoordinatorRepair=$false

function Add-Check([string]$Name, [string]$Status, [string]$Detail) {
    $Detail=Protect-WorkerSetupOutput $Detail
    $Checks.Add([pscustomobject]@{ name=$Name; status=$Status; detail=$Detail })
    Write-WorkerSetupLine ('[{0}] {1}: {2}' -f $Status.ToUpperInvariant(), $Name, $Detail)
}
function Native([string]$File, [string[]]$Arguments, [string]$Name='Native setup command', [int]$TimeoutSeconds=1800) {
    Invoke-WorkerNativeProgress -File $File -Arguments $Arguments -Name $Name -TimeoutSeconds $TimeoutSeconds
}
function Atomic-Json([string]$Path, $Value) { Write-WorkerJson -Path $Path -Value $Value }
function Fetch-Verified([string]$Url, [string]$Destination, [string]$Sha256) {
    for ($attempt=1;$attempt -le 3;$attempt++) {
        try {
            Get-WorkerVerifiedFile -Destination $Destination -Sha256 $Sha256 -Download {
                param($partial)
                Get-WorkerSetupDownload $Url $partial -Name ('Download '+[IO.Path]::GetFileName($Destination))
            }
            return
        } catch {
            if ($attempt -eq 3) { throw }
            Add-Check 'Download' 'repair' 'Retrying the locked dependency after a failed transfer or checksum check.'
            Start-Sleep -Seconds ($attempt*2)
        }
    }
}
function Find-Toolchain {
    $script:Compiler = $null; $script:CMake = $null; $script:Sdk = $null
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $found = Invoke-WorkerNativeProgress $vswhere @('-latest','-products','*','-version','[17.10,18.0)','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-format','json') -Name 'Inspect Microsoft toolchain' -TimeoutSeconds 30 -CaptureOutput
        $instances = @($found | ConvertFrom-Json)
        if ($instances.Count -gt 0) { $script:Compiler = [string]$instances[0].installationPath }
    }
    if ($script:Compiler) {
        $bundled = Join-Path $script:Compiler 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        if (Test-Path -LiteralPath $bundled -PathType Leaf) { $script:CMake = $bundled }
    }
    if (-not $script:CMake) {
        $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
        if ($command) { $script:CMake = $command.Source }
    }
    $kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
    $include = Join-Path $kits 'Include'
    if (Test-Path -LiteralPath $include) {
        foreach ($sdk in (Get-ChildItem -LiteralPath $include -Directory | Sort-Object Name -Descending)) {
            $lib = Join-Path $kits ('Lib\'+$sdk.Name+'\um\x64\winsqlite3.lib')
            if ((Test-Path (Join-Path $sdk.FullName 'um\winhttp.h')) -and (Test-Path -LiteralPath $lib)) { $script:Sdk=$sdk.Name; break }
        }
    }
    if ($script:CMake) {
        try {$version = Invoke-WorkerNativeProgress $script:CMake @('--version') -Name 'Inspect CMake' -TimeoutSeconds 30 -CaptureOutput}
        catch {$script:CMake=$null;return}
        if (($version -join "`n") -notmatch 'cmake version (\d+\.\d+\.\d+)') { $script:CMake=$null }
        elseif ([version]$Matches[1] -lt [version]'3.25.0') { $script:CMake=$null }
    }
}
function Is-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}
function Start-ElevatedBuildTools {
    $ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $arguments='-NoLogo -NoProfile -ExecutionPolicy Bypass -File "'+$PSCommandPath+'" -InstallMissing -AcceptToolLicenses -ElevatedBuildToolsOnly -NonInteractive'
    Add-Check 'Build tools' 'info' 'Requesting elevation only for Microsoft tools. This normal-user setup will resume afterward.'
    Write-WorkerSetupLine '[WAITING FOR INPUT] Approve the Windows UAC prompt to install Microsoft tools.'
    $process=Start-Process -FilePath $ps -Verb RunAs -ArgumentList $arguments -PassThru
    $code=Wait-WorkerSetupProcess $process 'Wait for elevated Microsoft tool setup'
    if ($code -ne 0) { throw ('Build-tools installation failed, requires reboot, or was cancelled (exit {0}). Rerun this same setup after resolving it.' -f $code) }
    Find-Toolchain
}
function Install-BuildTools($Lock) {
    if (-not $AcceptToolLicenses) { throw 'Use -AcceptToolLicenses with -InstallMissing only after accepting the Microsoft Build Tools license.' }
    if (-not (Is-Administrator)) { throw 'Installing Build Tools requires Administrator PowerShell. Install there, then rerun normal-user setup for the worker.' }
    $cache = Join-Path $env:TEMP 'OrganizerWorkerBootstrap'
    New-Item -ItemType Directory -Force -Path $cache | Out-Null
    $installer = Join-Path $cache 'vs_BuildTools.exe'
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Get-WorkerSetupDownload $Lock.compiler.installer_url $installer -Name 'Download Microsoft Build Tools installer'
    $signature = Invoke-WorkerSetupStage 'Verify Microsoft installer signature' {Get-AuthenticodeSignature -LiteralPath $installer}
    if ($signature.Status -ne 'Valid' -or -not $signature.SignerCertificate -or
        $signature.SignerCertificate.Subject -notmatch '(^|,\s*)O=Microsoft Corporation(,|$)') {
        throw 'Build Tools installer is not validly signed by Microsoft Corporation. It will not be run.'
    }
    $args = @('--passive','--wait','--norestart','--add',$Lock.compiler.workload,'--add',$Lock.compiler.cmake_component,'--includeRecommended')
    $process = Start-Process -FilePath $installer -ArgumentList $args -PassThru
    $code=Wait-WorkerSetupProcess $process 'Wait for Microsoft tool installation'
    if ($code -eq 3010) { throw 'Build Tools installed but a reboot is required. Reboot Windows yourself, then rerun setup.' }
    if ($code -ne 0) { throw ('Build Tools installation failed: {0}' -f $code) }
    Find-Toolchain
}
function Configure-WorkerConverter([switch]$Force) {
    # A verified pinned converter is per-user, independent of PATH or system DLLs.
    if (-not $NoConversion) {
        $converterOk=$false
        if ($config.ffmpeg_path -and $config.ffmpeg_sha256 -and (Test-Path -LiteralPath $config.ffmpeg_path -PathType Leaf)) {
            $converterOk=(Get-WorkerFileSha256 $config.ffmpeg_path) -ieq $config.ffmpeg_sha256
        }
        if ($Force -or -not $converterOk) {
            $accepted=Join-Path $Root ('ffmpeg-license-'+$lock.ffmpeg.sha256+'.json')
            if (-not $AcceptConversionLicense -and -not (Test-Path -LiteralPath $accepted)) {
                if ($NonInteractive) { throw 'Conversion needs the pinned FFmpeg build. Review its license and supply -AcceptConversionLicense, or choose -NoConversion.' }
                Write-Host ('FFmpeg '+$lock.ffmpeg.version+' will be installed for this user. '+$lock.ffmpeg.license)
                Write-Host $lock.ffmpeg.license_url
                if ((Read-WorkerSetupInput 'Type INSTALL to accept this dependency and enable conversion, or Enter to cancel') -cne 'INSTALL') { throw 'Conversion installation cancelled. Use -NoConversion for an explicit pairing-only setup.' }
            }
            $zip=Join-Path $cache ('ffmpeg-'+$lock.ffmpeg.sha256+'.zip')
            Fetch-Verified $lock.ffmpeg.url $zip $lock.ffmpeg.sha256
            $ffmpegDir=Join-Path $dependencies ('ffmpeg-'+$lock.ffmpeg.sha256.Substring(0,16))
            if ($Force -and (Test-Path -LiteralPath $ffmpegDir)) { Remove-WorkerGeneratedTree $ffmpegDir }
            Expand-WorkerDependency -Directory $ffmpegDir -ArchiveSha256 $lock.ffmpeg.sha256 -Extract {
                param($stage)
                $unpack=Join-Path $cache ('ffmpeg-unpack-'+[guid]::NewGuid().ToString('N'))
                try {
                    $ps=Join-Path $PSHOME 'powershell.exe'
                    Native $ps @('-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'setup-extract-zip.ps1'),'-Archive',$zip,'-Destination',$unpack) -Name 'Extract FFmpeg ZIP' -TimeoutSeconds 1800
                    $inner=Join-Path $unpack $lock.ffmpeg.directory
                    if (-not (Test-Path -LiteralPath (Join-Path $inner $lock.ffmpeg.executable))) { throw 'Pinned FFmpeg executable is absent from the archive.' }
                    Move-WorkerDependencyContents $inner $stage
                } finally { if (Test-Path -LiteralPath $unpack) { Remove-WorkerGeneratedTree $unpack } }
            }
            $config.ffmpeg_path=Join-Path $ffmpegDir $lock.ffmpeg.executable
            $config.ffmpeg_sha256=(Get-WorkerFileSha256 $config.ffmpeg_path).ToLowerInvariant()
            Atomic-Json $accepted @{archive_sha256=$lock.ffmpeg.sha256;accepted_utc=[DateTime]::UtcNow.ToString('o')}
        }
    }
}

try {
    if ($env:OS -ne 'Windows_NT') { throw 'This setup script targets Windows. Linux can run the portable contract tests via scripts/test.sh.' }
    if ($env:PROCESSOR_ARCHITECTURE -ne 'AMD64' -or -not [Environment]::Is64BitOperatingSystem -or -not [Environment]::Is64BitProcess) { throw 'This package supports x64 Windows 10 build 19041 or later. ARM64 and 32-bit Windows need a separate native package; setup will not install incompatible binaries.' }
    Add-Check 'PowerShell / architecture' 'pass' ('PowerShell {0}, x64' -f $PSVersionTable.PSVersion)
    $required = 'Get-CimInstance','Get-FileHash','Get-AuthenticodeSignature','Invoke-WebRequest','ConvertFrom-Json'
    foreach ($command in $required) {
        if (-not (Get-Command $command -ErrorAction SilentlyContinue)) { throw "Required Windows command unavailable: $command" }
    }
    $os = Invoke-WorkerSetupStage 'Inspect Windows and memory' {Get-CimInstance Win32_OperatingSystem}
    $cpu = @(Invoke-WorkerSetupStage 'Inspect CPU' {Get-CimInstance Win32_Processor})
    if ([int]$os.BuildNumber -lt 19041) { throw ('Windows build {0} is below this worker''s minimum 19041. No changes made.' -f $os.BuildNumber) }
    Add-Check 'Windows' 'pass' ($os.Caption + ' build ' + $os.BuildNumber)
    $ramMb = [math]::Floor([double]$os.TotalVisibleMemorySize / 1024)
    Add-Check 'Memory' 'pass' ('{0} MB installed; {1} MB currently free' -f $ramMb,[math]::Floor([double]$os.FreePhysicalMemory/1024))
    Add-Check 'CPU' 'pass' (($cpu | ForEach-Object { $_.Name }) -join '; ')
    try {
        $gpu = @(Invoke-WorkerSetupStage 'Inspect GPU inventory' {Get-CimInstance Win32_VideoController | ForEach-Object { $_.Name }})
        Add-Check 'GPU' 'info' (($gpu -join '; ') + '; detected only, not verified or enabled for compute')
    } catch { Add-Check 'GPU' 'info' 'GPU inventory unavailable. CPU operation does not require it.' }
    $drive = [IO.DriveInfo]::new([IO.Path]::GetPathRoot($Root))
    $neededGb = 3
    if ($InstallMissing) { $neededGb=15 }
    if ($drive.AvailableFreeSpace -lt ($neededGb * 1GB)) { throw "At least $neededGb GB free disk required for this setup mode." }
    Add-Check 'Disk' 'pass' ('{0:N1} GB free' -f ($drive.AvailableFreeSpace/1GB))
    foreach ($dll in 'winhttp.dll','bcrypt.dll','crypt32.dll','winsqlite3.dll') {
        if (-not (Test-Path -LiteralPath (Join-Path $env:SystemRoot "System32\$dll"))) { throw "Required Windows component missing: $dll. Repair Windows; do not download random DLLs." }
    }
    Add-Check 'Windows runtime components' 'pass' 'Required OS libraries present; executable doctor will test crypto, journal and storage APIs.'
    $sshd = Get-Service sshd -ErrorAction SilentlyContinue
    if ($sshd) { Add-Check 'SSH (optional)' 'info' ([string]$sshd.Status) }
    else { Add-Check 'SSH (optional)' 'info' 'Not installed or still installing. Setup will not alter SSH or firewall rules.' }
    $lock = Get-Content -LiteralPath (Join-Path $Source 'config\dependencies.lock.json') -Raw | ConvertFrom-Json
    Find-Toolchain
    if ($Compiler) { Add-Check 'MSVC' 'pass' $Compiler }
    else { Add-Check 'MSVC' 'missing' 'Visual Studio 2022 >=17.10 with C++ tools and Windows SDK required.' }
    if ($CMake) { Add-Check 'CMake' 'pass' $CMake }
    else { Add-Check 'CMake' 'missing' 'CMake >=3.25 required; Build Tools CMake component can supply it.' }
    if ($Sdk) { Add-Check 'Windows SDK' 'pass' $Sdk }
    else { Add-Check 'Windows SDK' 'missing' 'Windows 10/11 SDK headers and x64 WinSQLite import library are required.' }
    if ($CheckOnly) {
        Add-Check 'Check-only' 'info' 'No tools installed, downloads performed, configuration changed, or jobs started.'
        if (-not $Compiler -or -not $CMake -or -not $Sdk) { exit 2 }
        exit 0
    }
    if ((Is-Administrator) -and -not $ElevatedBuildToolsOnly) {
        throw 'Double-click setup.cmd as your normal Windows user. It requests elevation only when needed; worker identity must not be created as Administrator.'
    }
    if ($ElevatedBuildToolsOnly -and -not (Is-Administrator)) { throw 'The toolchain helper requires elevation.' }
    if (-not $Compiler -or -not $CMake -or -not $Sdk) {
        if (-not $InstallMissing -or -not $AcceptToolLicenses) {
            if ($NonInteractive) { throw 'Missing Microsoft build tools. Supply -InstallMissing -AcceptToolLicenses after reviewing the license, or run setup interactively.' }
            Write-Host 'Microsoft C++ Build Tools and the Windows SDK are required.'
            Write-Host 'Review Microsoft Visual Studio license terms: https://visualstudio.microsoft.com/license-terms/'
            $answer=Read-WorkerSetupInput 'Type INSTALL to accept those terms and install the missing tools, or press Enter to cancel'
            if ($answer -cne 'INSTALL') { throw 'Tool installation cancelled. No Microsoft tools were installed.' }
            $InstallMissing=$true;$AcceptToolLicenses=$true
        }
        if ($ElevatedBuildToolsOnly) { Install-BuildTools $lock }
        else { Start-ElevatedBuildTools }
        if (-not $Compiler -or -not $CMake -or -not $Sdk) { throw 'Toolchain remains incomplete. Repair the Microsoft C++ workload, Windows SDK and CMake, then rerun setup.' }
    }
    if ($ElevatedBuildToolsOnly) {
        Add-Check 'Build tools' 'pass' 'Toolchain step complete; returning to normal-user setup.'
        exit 0
    }
    New-Item -ItemType Directory -Force -Path $Root | Out-Null
    # Do not kill an active worker during update. The user's tray Exit is authoritative.
    $lockFile = Join-Path $Root 'instance.lock'
    try { $script:InstallGuard=[IO.File]::Open($lockFile,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None) }
    catch { throw 'Worker is running. Use its tray Exit before installing or updating.' }
    $sid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $icacls = Join-Path $env:SystemRoot 'System32\icacls.exe'
    Native $icacls @($Root,'/inheritance:r','/grant:r',('*'+$sid+':(OI)(CI)F'),'*S-1-5-18:(OI)(CI)F') -Name 'Protect per-user worker files' -TimeoutSeconds 900
    foreach ($sub in 'versions','jobs','probes','logs') { New-Item -ItemType Directory -Force -Path (Join-Path $Root $sub) | Out-Null }
    $script:WorkerSetupProgressLog=Join-Path $Root 'logs\setup-progress.log'
    Write-WorkerSetupLine ('[INFO] Live progress log: '+$script:WorkerSetupProgressLog)
    $configPath = Join-Path $Root 'worker.json'
    if (Test-Path -LiteralPath $configPath) { $config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json }
    else { $config = Get-Content -LiteralPath (Join-Path $Source 'config\worker.example.json') -Raw | ConvertFrom-Json
        $config.limits.ram_mb=[int][math]::Max(512,[math]::Min(4096,[math]::Floor($ramMb/3)))
        $config.limits.cpu_threads=[int][math]::Max(1,[math]::Min(4,[Environment]::ProcessorCount-2))
    }
    $previousOrigin=[string]$config.coordinator_url
    $hasIdentity=(Test-Path -LiteralPath (Join-Path $Root 'credential.dpapi')) -or (Test-Path -LiteralPath (Join-Path $Root 'pending-pairing.dpapi')) -or (Test-Path -LiteralPath (Join-Path $Root 'journal.sqlite3'))
    if ($Server -and $hasIdentity -and -not $RePair) {
        $requestedOrigin=ConvertTo-WorkerOrigin $Server
        $sameOrigin=$false
        try { $sameOrigin=(ConvertTo-WorkerOrigin $previousOrigin) -eq $requestedOrigin } catch { }
        if (-not $sameOrigin -and -not $NonInteractive) {
            Write-Host ('This installation is paired with '+$previousOrigin+'. The requested endpoint is '+$requestedOrigin+'.')
            if ((Read-WorkerSetupInput 'Type REPAIR to explicitly archive the old coordinator identity and pair here, or Enter to cancel') -ceq 'REPAIR') { $RePair=$true }
        }
    }
    $config.coordinator_url=Resolve-WorkerOrigin -Stored ([string]$config.coordinator_url) -Requested $Server -HasIdentity $hasIdentity -RePair:$RePair -NonInteractive:$NonInteractive -Prompt {
        Read-WorkerSetupInput 'Lightsail worker HTTPS endpoint (https://your-hostname; no default)'
    }
    if (-not $RePair) {
        $action=Get-WorkerPairingAction -Origin $config.coordinator_url -NowMs ([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()) -ReadSecret {param($slot)Read-WorkerProtectedRecord $Root $slot}
        if ($action.kind -ne 'none') {
            Add-Check 'Saved pairing' 'action-required' $action.reason
            if ($NonInteractive) { throw 'Saved pairing needs explicit recovery. Rerun interactively or supply -RePair after reviewing the retained state.' }
            if ($action.kind -eq 'archive') {
                if ((Read-WorkerSetupInput 'Type ARCHIVE to keep a backup of this entire coordinator scope and pair from scratch') -cne 'ARCHIVE') { throw 'Identity recovery cancelled; all existing bytes retained.' }
                $script:WholeCoordinatorRepair=$true
            } elseif ((Read-WorkerSetupInput 'Type PAIR to explicitly request a fresh pairing code using the existing key') -cne 'PAIR') { throw 'New pairing cancelled; previous state retained.' }
            $RePair=$true
        }
    }
    try {
        if ((ConvertTo-WorkerOrigin $previousOrigin) -ne $config.coordinator_url -and -not $PSBoundParameters.ContainsKey('StorageHosts')) { $config.storage_hosts=@() }
    } catch { if (-not $PSBoundParameters.ContainsKey('StorageHosts')) { $config.storage_hosts=@() } }
    if ($PSBoundParameters.ContainsKey('StorageHosts')) {
        foreach ($hostName in $StorageHosts) { if ($hostName -notmatch '^[a-zA-Z0-9][a-zA-Z0-9.-]*[a-zA-Z0-9]$' -or $hostName.Contains('..')) { throw "Invalid storage host: $hostName" } }
        $config.storage_hosts=@($StorageHosts | ForEach-Object { $_.ToLowerInvariant() } | Select-Object -Unique)
    }
    # Discover exact storage hosts through the selected HTTPS coordinator. Keep
    # certificate checking and redirect refusal enabled; host approval is explicit.
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $protocol=$null
    try {
        Invoke-WorkerRepair -Operation {
            $reply=Invoke-WorkerSetupStage 'Check coordinator HTTPS protocol' {
                Invoke-WebRequest -Uri ($config.coordinator_url+'/v1/worker/protocol') -UseBasicParsing -MaximumRedirection 0 -TimeoutSec 15
            }
            $script:ProtocolReply=$reply.Content | ConvertFrom-Json
            if ($script:ProtocolReply.protocol -ne 1) { throw 'Coordinator protocol is incompatible.' }
        } -Repair { Add-Check 'Coordinator' 'repair' 'Retrying HTTPS readiness once after a failed network probe.' }
        $protocol=$script:ProtocolReply
        if ($protocol.PSObject.Properties.Name -contains 'storage_hosts') {
            $advertised=@($protocol.storage_hosts)
            if ($advertised.Count -gt 8) { throw 'Coordinator advertised too many storage hosts.' }
            foreach ($hostName in $advertised) {
                if ($hostName -notmatch '^[a-zA-Z0-9][a-zA-Z0-9.-]*[a-zA-Z0-9]$' -or $hostName.Contains('..')) { throw 'Coordinator advertised an invalid storage host.' }
            }
            $missing=@($advertised | Where-Object {$_ -notin $config.storage_hosts})
            if ($missing.Count -gt 0) {
                Write-Host ('Coordinator storage hosts: '+($missing -join ', '))
                if (-not $AcceptStorageHosts) {
                    if ($NonInteractive) { throw 'New storage hosts require -AcceptStorageHosts or explicit -StorageHosts.' }
                    if ((Read-WorkerSetupInput 'Type ALLOW to approve HTTPS transfers to these exact hosts') -cne 'ALLOW') { throw 'Storage host approval cancelled.' }
                }
                $config.storage_hosts=@($config.storage_hosts+$missing | Select-Object -Unique)
            }
        }
        if (($protocol.PSObject.Properties.Name -contains 'job_dispatch') -and $protocol.job_dispatch) {
            Add-Check 'Coordinator' 'pass' 'Remote job dispatch is enabled; approved storage hosts are configured.'
        } else { Add-Check 'Coordinator' 'not-ready' 'Pairing endpoint is ready; remote job dispatch is disabled on the server.' }
    } catch {
        # Missing/changed transfer authorization cannot be silently waved through.
        throw ('Coordinator setup did not complete: '+$_.Exception.Message+'. Rerun the same setup after connectivity/server configuration is restored.')
    }
    if ($PSBoundParameters.ContainsKey('FfmpegPath')) {
        if (-not $FfmpegSha256 -or $FfmpegSha256 -notmatch '^[a-fA-F0-9]{64}$') { throw 'Supplying FFmpeg requires -FfmpegSha256 from your approved download. Setup will not trust a filename or PATH entry.' }
        $exe=(Resolve-Path -LiteralPath $FfmpegPath).Path
        if ([IO.Path]::GetExtension($exe) -ine '.exe') { throw 'FFmpeg must be an approved native .exe, not a shell script.' }
        if ((Get-WorkerFileSha256 $exe) -ine $FfmpegSha256) { throw 'FFmpeg checksum mismatch. Not installed or enabled.' }
        $config.ffmpeg_path=$exe; $config.ffmpeg_sha256=$FfmpegSha256.ToLowerInvariant()
    }
    $freeForJobs=[math]::Max(0,[math]::Floor([double]$os.FreePhysicalMemory/1024)-[double]$config.limits.reserve_ram_mb)
    Add-Check 'Job capacity' 'info' ('{0} MB currently available after the {1} MB Windows reserve; video offers need 512 MB, image offers 256 MB. Existing limits are retained.' -f $freeForJobs,$config.limits.reserve_ram_mb)
    # Dependencies come from the reviewed lock file, never an unpinned "latest" URL.
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $cache=Join-Path $Root 'downloads'; New-Item -ItemType Directory -Force -Path $cache | Out-Null
    $dependencies=Join-Path $Root 'dependencies';New-Item -ItemType Directory -Force -Path $dependencies | Out-Null
    $archive=Join-Path $cache ('boost-'+$lock.boost.version+'.tar.gz')
    Fetch-Verified $lock.boost.url $archive $lock.boost.sha256
    $boost=Join-Path $dependencies $lock.boost.directory
    Expand-WorkerDependency -Directory $boost -ArchiveSha256 $lock.boost.sha256 -Extract {
        param($stage)
        $unpack=Join-Path $cache ('boost-unpack-'+[guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $unpack | Out-Null
        Push-Location $unpack
        try {
            Native $CMake @('-E','tar','xzf',$archive) -Name 'Extract Boost archive' -TimeoutSeconds 1800
            $inner=Join-Path $unpack $lock.boost.directory
            if (-not (Test-Path -LiteralPath (Join-Path $inner 'boost\json\src.hpp'))) { throw 'Verified Boost archive is incomplete.' }
            Move-WorkerDependencyContents $inner $stage
        } finally { Pop-Location;Remove-WorkerGeneratedTree $unpack }
    }
    if (-not $PSBoundParameters.ContainsKey('FfmpegPath')) { Configure-WorkerConverter }
    if ($NoConversion) { $config.ffmpeg_path='';$config.ffmpeg_sha256='' }
    $build=Join-Path $Root 'build-windows-x64'
    $ctest=Join-Path (Split-Path -Parent $CMake) 'ctest.exe'
    Invoke-WorkerRepair -Operation {
        Native $CMake @('-S',$Source,'-B',$build,'-G','Visual Studio 17 2022','-A','x64',('-DCMAKE_GENERATOR_INSTANCE='+$Compiler),('-DOW_BOOST_ROOT='+$boost),'-DBUILD_TESTING=ON') -Name 'Configure native Windows build' -TimeoutSeconds 600
        Native $CMake @('--build',$build,'--config','Release','--parallel','2') -Name 'Compile native Windows worker' -TimeoutSeconds 3600
    } -Repair {
        Add-Check 'Build' 'repair' 'Removing only generated build files, then configuring and compiling from verified sources again.'
        if (Test-Path -LiteralPath $build) { Remove-WorkerGeneratedTree $build }
    }
    # A behavior-test failure is a defect, not permission to skip a gate or reinstall tools.
    Native $ctest @('--test-dir',$build,'-C','Release','--output-on-failure') -Name 'Run native behavior tests' -TimeoutSeconds 900
    Add-Check 'Build and behavior tests' 'pass' 'Native compilation and all registered CTest contracts passed.'
    $built=Join-Path $build 'Release\OrganizerWorker.exe'
    if (-not (Test-Path -LiteralPath $built)) { throw 'Build reported success but no native worker executable exists.' }
    $buildId=(Get-WorkerFileSha256 $built).Substring(0,16).ToLowerInvariant()
    $version=Join-Path $Root ('versions\0.1.2-'+$buildId)
    Native $CMake @('--install',$build,'--config','Release','--prefix',$version) -Name 'Install verified native binaries' -TimeoutSeconds 120
    foreach ($binary in 'OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe') { if (-not (Test-Path -LiteralPath (Join-Path $version $binary))) { throw "Installed binary missing: $binary" } }
    # Validate candidate configuration without publishing over the installed one.
    $doctor=Join-Path $version 'OrganizerWorkerDoctor.exe'
    $stage=Join-Path $Root ('install-check-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stage | Out-Null
    try {
        Invoke-WorkerRepair -Operation {
            Atomic-Json (Join-Path $stage 'worker.json') $config
            $healthText=Invoke-WorkerNativeProgress $doctor @('--json','--probe-ffmpeg','--data-root',$stage) -Name 'Probe runtime and sample conversions' -TimeoutSeconds 180 -CaptureOutput
            $script:DoctorHealth=($healthText -join "`n") | ConvertFrom-Json
            if (-not $script:DoctorHealth.runtime_healthy) { throw 'Runtime doctor did not confirm health.' }
            if (-not $NoConversion -and -not $script:DoctorHealth.processing_ready) { throw ('Converter probe failed: '+$script:DoctorHealth.capabilities.reason) }
        } -Repair {
            if ($script:ExplicitFfmpeg -or $NoConversion) { throw 'Explicit converter/runtime check failed; it cannot be repaired by changing the approved configuration.' }
            Add-Check 'Conversion adapters' 'repair' 'Re-extracting the pinned converter and retrying sample conversions once.'
            Configure-WorkerConverter -Force
        }
        $health=$script:DoctorHealth
    } finally { Remove-WorkerGeneratedTree $stage }
    Add-Check 'Runtime self-check' 'pass' 'Windows crypto, atomic storage and SQLite journal passed.'
    if ($health.processing_ready) { Add-Check 'Conversion adapters' 'pass' 'Approved FFmpeg passed real sample conversions.' }
    elseif ($NoConversion) { Add-Check 'Conversion adapters' 'disabled' 'Pairing-only mode was explicitly selected.' }
    else { throw ('Configured converter failed a real functional probe: '+$health.capabilities.reason+'. Conversion setup is incomplete; no new configuration was published.') }
    if ($RePair) {
        $wholeCoordinator=$true
        try { $wholeCoordinator=$script:WholeCoordinatorRepair -or ((ConvertTo-WorkerOrigin $previousOrigin) -ne $config.coordinator_url) } catch { }
        if (-not $wholeCoordinator -and (Test-Path -LiteralPath (Join-Path $Root 'journal.sqlite3'))) {
            $bound=Invoke-WorkerNativeProgress $doctor @('--json','--bind-journal','--data-root',$Root) -Name 'Verify journal endpoint before re-pairing' -TimeoutSeconds 30 -CaptureOutput
        }
        $archived=Invoke-WorkerSetupStage 'Archive explicitly selected pairing state' {
            $script:WorkerSetupProgressLog=$null
            Archive-WorkerPairing -Root $Root -WholeCoordinator:$wholeCoordinator
        }
        # Whole-coordinator archival moves logs too; recreate the new log scope.
        New-Item -ItemType Directory -Force -Path (Join-Path $Root 'logs') | Out-Null
        $script:WorkerSetupProgressLog=Join-Path $Root 'logs\setup-progress.log'
        if ($archived) { Add-Check 'Pairing' 'info' 'Previous state archived locally; revoke the old credential on its coordinator. Endpoint changes also isolate the old journal, keys and media.' }
    }
    Atomic-Json $configPath $config
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'setup-progress.ps1') -Destination (Join-Path $Root 'setup-progress.ps1') -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'setup-policy.ps1') -Destination (Join-Path $Root 'setup-policy.ps1') -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'start-worker.ps1') -Destination (Join-Path $Root 'start-worker.ps1') -Force
    $current=[ordered]@{ schema=1; directory=$version; sha256=(Get-WorkerFileSha256 (Join-Path $version 'OrganizerWorker.exe')).ToLowerInvariant() }
    Atomic-Json (Join-Path $Root 'current.json') $current
    $complete=Complete-WorkerSetup -NoStartup:$NoStartup -NoLaunch:$NoLaunch -RegisterStartup {
        Register-WorkerStartup -Root $Root -Sid $sid
    } -VerifyStartup {
        Test-WorkerStartup -Root $Root -Sid $sid
    } -LaunchWorker {
        $script:InstallGuard.Dispose();$script:InstallGuard=$null
        & (Join-Path $Root 'start-worker.ps1')
    }
    if ($complete.startup_registered) { Add-Check 'Startup' 'pass' 'Verified normal-user startup at Windows sign-in (Task Scheduler or per-user Startup shortcut).' }
    if ($complete.started) { Add-Check 'Launch' 'pass' 'Worker process and readiness marker verified. Pairing/processing readiness is reported separately.' }
    Add-Check 'Installed' 'pass' $version
    if (-not $config.coordinator_url) { Add-Check 'Lightsail' 'not-configured' 'Controller can run offline. Configure a coordinator only after its worker protocol and pairing endpoints exist.' }
    else {
        try {
            $coordinatorText=Invoke-WorkerNativeProgress $doctor @('--json','--check-coordinator','--data-root',$Root) -Name 'Verify coordinator reachability' -TimeoutSeconds 45 -CaptureOutput
            Add-Check 'Lightsail' 'pass' 'Coordinator advertises protocol v1. Pairing approval is still required.'
        } catch { Add-Check 'Lightsail' 'not-ready' 'Worker protocol probe failed. No production job was submitted; pairing is not confirmed.' }
    }
    Write-Host ('Start with: & "{0}"' -f (Join-Path $Root 'start-worker.ps1'))
    exit 0
} catch {
    Add-Check 'Setup' 'failed' $_.Exception.Message
    exit 1
} finally {
    if ($script:InstallGuard) { $script:InstallGuard.Dispose();$script:InstallGuard=$null }
    try {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ReportPath) | Out-Null
        Atomic-Json $ReportPath ([ordered]@{ recorded_utc=[DateTime]::UtcNow.ToString('o'); checks=@($Checks.ToArray()) })
        Write-Host "Report: $ReportPath"
        $localReport=Join-Path $Root 'logs\setup-report.json'
        Atomic-Json $localReport ([ordered]@{recorded_utc=[DateTime]::UtcNow.ToString('o');checks=@($Checks.ToArray())})
        Write-Host "Local report: $localReport"
    } catch {
        try {
            $fallback=Join-Path ([IO.Path]::GetTempPath()) 'OrganizerWorker-setup-report.json'
            Atomic-Json $fallback ([ordered]@{recorded_utc=[DateTime]::UtcNow.ToString('o');checks=@($Checks.ToArray())})
            Write-Host "Report: $fallback"
        } catch { Write-Warning ('Could not write diagnostic report: '+$_.Exception.Message) }
    }
}
