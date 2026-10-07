#requires -Version 5.1
[CmdletBinding()]
param(
    [switch]$CheckOnly,
    [switch]$InstallMissing,
    [switch]$AcceptToolLicenses,
    [string]$Server,
    [string[]]$StorageHosts,
    [string]$FfmpegPath,
    [string]$FfmpegSha256,
    [switch]$StartAtLogon,
    [switch]$Launch
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$Source = Split-Path -Parent $PSScriptRoot
$Checks = New-Object System.Collections.Generic.List[object]
$Root = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'OrganizerWorker'
$ReportPath = Join-Path $Source 'verification\windows-setup.json'
$script:Compiler = $null
$script:CMake = $null
$script:Sdk = $null

function Add-Check([string]$Name, [string]$Status, [string]$Detail) {
    $Checks.Add([pscustomobject]@{ name=$Name; status=$Status; detail=$Detail })
    Write-Host ('[{0}] {1}: {2}' -f $Status.ToUpperInvariant(), $Name, $Detail)
}
function Native([string]$File, [string[]]$Arguments) {
    if (-not (Test-Path -LiteralPath $File -PathType Leaf)) { throw "Executable missing: $File" }
    $previous = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = & $File @Arguments 2>&1
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previous }
    $output | ForEach-Object { Write-Host $_ }
    if ($code -ne 0) { throw ('Command failed with exit code {0}: {1}' -f $code, $File) }
}
function Atomic-Json([string]$Path, $Value) {
    $temp = $Path + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    try {
        [IO.File]::WriteAllText($temp, ($Value | ConvertTo-Json -Depth 16), $utf8)
        if (Test-Path -LiteralPath $Path) {
            [IO.File]::Replace($temp, $Path, $null)
        } else { [IO.File]::Move($temp, $Path) }
    } finally { if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Force } }
}
function Fetch-Verified([string]$Url, [string]$Destination, [string]$Sha256) {
    if (Test-Path -LiteralPath $Destination) {
        if ((Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash -ieq $Sha256) { return }
        throw "Cached download checksum mismatch: $Destination. Remove that file and rerun."
    }
    $partial = $Destination + '.part'
    for ($attempt=1; $attempt -le 3; $attempt++) {
        try {
            Invoke-WebRequest -Uri $Url -OutFile $partial -UseBasicParsing -TimeoutSec 180
            if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash -ine $Sha256) {
                throw 'Downloaded source checksum mismatch; refusing to extract or execute it.'
            }
            Move-Item -LiteralPath $partial -Destination $Destination
            return
        } catch {
            if (Test-Path -LiteralPath $partial) { Remove-Item -LiteralPath $partial -Force }
            if ($attempt -eq 3) { throw }
            Start-Sleep -Seconds ($attempt * 2)
        }
    }
}
function Find-Toolchain {
    $script:Compiler = $null; $script:CMake = $null; $script:Sdk = $null
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $found = & $vswhere -latest -products '*' -version '[17.10,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json
        if ($LASTEXITCODE -ne 0) { throw 'vswhere could not inspect installed compilers.' }
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
        $version = & $script:CMake --version
        if ($LASTEXITCODE -ne 0 -or ($version -join "`n") -notmatch 'cmake version (\d+\.\d+\.\d+)') { $script:CMake=$null }
        elseif ([version]$Matches[1] -lt [version]'3.25.0') { $script:CMake=$null }
    }
}
function Is-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}
function Install-BuildTools($Lock) {
    if (-not $AcceptToolLicenses) { throw 'Use -AcceptToolLicenses with -InstallMissing only after accepting the Microsoft Build Tools license.' }
    if (-not (Is-Administrator)) { throw 'Installing Build Tools requires Administrator PowerShell. Install there, then rerun normal-user setup for the worker.' }
    $cache = Join-Path $env:TEMP 'OrganizerWorkerBootstrap'
    New-Item -ItemType Directory -Force -Path $cache | Out-Null
    $installer = Join-Path $cache 'vs_BuildTools.exe'
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $Lock.compiler.installer_url -OutFile $installer -UseBasicParsing -TimeoutSec 180
    $signature = Get-AuthenticodeSignature -LiteralPath $installer
    if ($signature.Status -ne 'Valid' -or -not $signature.SignerCertificate -or
        $signature.SignerCertificate.Subject -notmatch '(^|,\s*)O=Microsoft Corporation(,|$)') {
        throw 'Build Tools installer is not validly signed by Microsoft Corporation. It will not be run.'
    }
    $args = @('--passive','--wait','--norestart','--add',$Lock.compiler.workload,'--add',$Lock.compiler.cmake_component,'--includeRecommended')
    $process = Start-Process -FilePath $installer -ArgumentList $args -Wait -PassThru
    if ($process.ExitCode -eq 3010) { throw 'Build Tools installed but a reboot is required. Reboot Windows yourself, then rerun setup.' }
    if ($process.ExitCode -ne 0) { throw ('Build Tools installation failed: {0}' -f $process.ExitCode) }
    Find-Toolchain
}
try {
    if ($env:OS -ne 'Windows_NT') { throw 'This setup script targets Windows. Linux can run the portable contract tests via scripts/test.sh.' }
    if (-not [Environment]::Is64BitOperatingSystem -or -not [Environment]::Is64BitProcess) { throw 'Run x64 Windows PowerShell. setup.cmd automatically selects the native host.' }
    Add-Check 'PowerShell / architecture' 'pass' ('PowerShell {0}, x64' -f $PSVersionTable.PSVersion)
    $required = 'Get-CimInstance','Get-FileHash','Get-AuthenticodeSignature','Invoke-WebRequest','ConvertFrom-Json'
    foreach ($command in $required) {
        if (-not (Get-Command $command -ErrorAction SilentlyContinue)) { throw "Required Windows command unavailable: $command" }
    }
    $os = Get-CimInstance Win32_OperatingSystem
    $cpu = @(Get-CimInstance Win32_Processor)
    if ([int]$os.BuildNumber -lt 19041) { throw ('Windows build {0} is below this worker''s minimum 19041. No changes made.' -f $os.BuildNumber) }
    Add-Check 'Windows' 'pass' ($os.Caption + ' build ' + $os.BuildNumber)
    $ramMb = [math]::Floor([double]$os.TotalVisibleMemorySize / 1024)
    Add-Check 'Memory' 'pass' ('{0} MB installed; {1} MB currently free' -f $ramMb,[math]::Floor([double]$os.FreePhysicalMemory/1024))
    Add-Check 'CPU' 'pass' (($cpu | ForEach-Object { $_.Name }) -join '; ')
    try {
        $gpu = @(Get-CimInstance Win32_VideoController | ForEach-Object { $_.Name })
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
    if (-not $Compiler -or -not $CMake -or -not $Sdk) {
        if (-not $InstallMissing) { throw 'Missing build tools. Rerun in Administrator PowerShell with -InstallMissing -AcceptToolLicenses to install Microsoft tools; then rerun setup as your normal user.' }
        Install-BuildTools $lock
        if (-not $Compiler -or -not $CMake -or -not $Sdk) { throw 'Toolchain is still incomplete after installation. Open Visual Studio Installer and repair the C++ workload, Windows SDK and CMake component.' }
    }
    if (Is-Administrator) {
        Add-Check 'Build tools' 'pass' 'Toolchain available. Close Administrator PowerShell and rerun setup as the normal worker user. No worker identity created under Administrator.'
        exit 0
    }
    New-Item -ItemType Directory -Force -Path $Root | Out-Null
    # Do not kill an active worker during update. The user's tray Exit is authoritative.
    $lockFile = Join-Path $Root 'instance.lock'
    try { $exclusive=[IO.File]::Open($lockFile,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None); $exclusive.Dispose() }
    catch { throw 'Worker is running. Use its tray Exit before installing or updating.' }
    $sid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $icacls = Join-Path $env:SystemRoot 'System32\icacls.exe'
    Native $icacls @($Root,'/inheritance:r','/grant:r',('*'+$sid+':(OI)(CI)F'),'*S-1-5-18:(OI)(CI)F')
    foreach ($sub in 'versions','jobs','probes','logs') { New-Item -ItemType Directory -Force -Path (Join-Path $Root $sub) | Out-Null }
    $configPath = Join-Path $Root 'worker.json'
    if (Test-Path -LiteralPath $configPath) { $config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json }
    else { $config = Get-Content -LiteralPath (Join-Path $Source 'config\worker.example.json') -Raw | ConvertFrom-Json
        $config.limits.ram_mb=[int][math]::Max(512,[math]::Min(4096,[math]::Floor($ramMb/3)))
        $config.limits.cpu_threads=[int][math]::Max(1,[math]::Min(4,[Environment]::ProcessorCount-2))
    }
    if ($PSBoundParameters.ContainsKey('Server')) {
        $uri=$null
        if (-not [Uri]::TryCreate($Server,[UriKind]::Absolute,[ref]$uri) -or $uri.Scheme -ne 'https' -or $uri.Port -ne 443 -or $uri.UserInfo -or $uri.AbsolutePath -ne '/' -or $uri.Query -or $uri.Fragment) { throw '-Server must be an HTTPS origin on port 443, without path, query or credentials.' }
        if ((Test-Path (Join-Path $Root 'credential.dpapi')) -and $config.coordinator_url.TrimEnd('/') -ne $Server.TrimEnd('/')) { throw 'Existing pairing belongs to another coordinator. Do not silently change it. Revoke and explicitly re-pair first.' }
        $config.coordinator_url=$Server.TrimEnd('/')
    }
    if ($PSBoundParameters.ContainsKey('StorageHosts')) {
        foreach ($hostName in $StorageHosts) { if ($hostName -notmatch '^[a-zA-Z0-9][a-zA-Z0-9.-]*[a-zA-Z0-9]$' -or $hostName.Contains('..')) { throw "Invalid storage host: $hostName" } }
        $config.storage_hosts=@($StorageHosts | ForEach-Object { $_.ToLowerInvariant() } | Select-Object -Unique)
    }
    if ($PSBoundParameters.ContainsKey('FfmpegPath')) {
        if (-not $FfmpegSha256 -or $FfmpegSha256 -notmatch '^[a-fA-F0-9]{64}$') { throw 'Supplying FFmpeg requires -FfmpegSha256 from your approved download. Setup will not trust a filename or PATH entry.' }
        $exe=(Resolve-Path -LiteralPath $FfmpegPath).Path
        if ([IO.Path]::GetExtension($exe) -ine '.exe') { throw 'FFmpeg must be an approved native .exe, not a shell script.' }
        if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ine $FfmpegSha256) { throw 'FFmpeg checksum mismatch. Not installed or enabled.' }
        $config.ffmpeg_path=$exe; $config.ffmpeg_sha256=$FfmpegSha256.ToLowerInvariant()
    }
    # Dependencies come from the reviewed lock file, never an unpinned "latest" URL.
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $cache=Join-Path $Source '.cache'; New-Item -ItemType Directory -Force -Path $cache | Out-Null
    $archive=Join-Path $cache ('boost-'+$lock.boost.version+'.tar.gz')
    Fetch-Verified $lock.boost.url $archive $lock.boost.sha256
    $boost=Join-Path $cache $lock.boost.directory
    if (-not (Test-Path -LiteralPath (Join-Path $boost 'boost\json.hpp'))) {
        Push-Location $cache
        try { Native $CMake @('-E','tar','xzf',$archive) } finally { Pop-Location }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $boost 'boost\json\src.hpp'))) { throw 'Verified Boost extraction is incomplete.' }
    $build=Join-Path $Source 'build-windows-x64'
    Native $CMake @('-S',$Source,'-B',$build,'-G','Visual Studio 17 2022','-A','x64',('-DCMAKE_GENERATOR_INSTANCE='+$Compiler),('-DOW_BOOST_ROOT='+$boost),'-DBUILD_TESTING=ON')
    Native $CMake @('--build',$build,'--config','Release','--parallel','2')
    $ctest=Join-Path (Split-Path -Parent $CMake) 'ctest.exe'
    Native $ctest @('--test-dir',$build,'-C','Release','--output-on-failure')
    Add-Check 'Build and behavior tests' 'pass' 'Native compilation and all registered CTest contracts passed.'
    $built=Join-Path $build 'Release\OrganizerWorker.exe'
    if (-not (Test-Path -LiteralPath $built)) { throw 'Build reported success but no native worker executable exists.' }
    $buildId=(Get-FileHash -LiteralPath $built -Algorithm SHA256).Hash.Substring(0,16).ToLowerInvariant()
    $version=Join-Path $Root ('versions\0.1.0-'+$buildId)
    Native $CMake @('--install',$build,'--config','Release','--prefix',$version)
    foreach ($binary in 'OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe') { if (-not (Test-Path -LiteralPath (Join-Path $version $binary))) { throw "Installed binary missing: $binary" } }
    Atomic-Json $configPath $config
    $doctor=Join-Path $version 'OrganizerWorkerDoctor.exe'
    $healthText=& $doctor --json --probe-ffmpeg
    if ($LASTEXITCODE -ne 0) { throw ('Runtime self-check failed: '+($healthText -join "`n")) }
    $health=($healthText -join "`n") | ConvertFrom-Json
    if (-not $health.runtime_healthy) { throw 'Runtime doctor did not confirm health.' }
    Add-Check 'Runtime self-check' 'pass' 'Windows crypto, atomic storage and SQLite journal passed.'
    if ($health.processing_ready) { Add-Check 'Conversion adapters' 'pass' 'Approved FFmpeg passed real sample conversions.' }
    else { Add-Check 'Conversion adapters' 'disabled' 'No verified converter. Worker can launch and pair but will not accept conversion jobs.' }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'start-worker.ps1') -Destination (Join-Path $Root 'start-worker.ps1') -Force
    $current=[ordered]@{ schema=1; directory=$version; sha256=(Get-FileHash -LiteralPath (Join-Path $version 'OrganizerWorker.exe') -Algorithm SHA256).Hash.ToLowerInvariant() }
    Atomic-Json (Join-Path $Root 'current.json') $current
    if ($StartAtLogon) {
        foreach ($command in 'New-ScheduledTaskAction','New-ScheduledTaskTrigger','New-ScheduledTaskPrincipal','New-ScheduledTaskSettingsSet','Register-ScheduledTask') { if (-not (Get-Command $command -ErrorAction SilentlyContinue)) { throw "Startup requested, but Windows ScheduledTasks command is unavailable: $command" } }
        $ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
        $launcher=Join-Path $Root 'start-worker.ps1'
        $action=New-ScheduledTaskAction -Execute $ps -Argument ('-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "'+$launcher+'"')
        $user=[Security.Principal.WindowsIdentity]::GetCurrent().Name
        $trigger=New-ScheduledTaskTrigger -AtLogOn -User $user
        $principal=New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Limited
        $settings=New-ScheduledTaskSettingsSet -ExecutionTimeLimit ([TimeSpan]::Zero)
        Register-ScheduledTask -TaskName ('OrganizerWorker-'+$sid) -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description 'User-controlled Organizer worker; tray Exit stops it.' -Force | Out-Null
        Add-Check 'Startup' 'pass' 'Registered for this user''s interactive logon, not a hidden system service.'
    }
    Add-Check 'Installed' 'pass' $version
    if (-not $config.coordinator_url) { Add-Check 'Lightsail' 'not-configured' 'Controller can run offline. Configure a coordinator only after its worker protocol and pairing endpoints exist.' }
    else {
        $coordinatorText=& $doctor --json --check-coordinator
        if ($LASTEXITCODE -ne 0) { Add-Check 'Lightsail' 'not-ready' 'Worker protocol probe failed. No production job was submitted; pairing is not confirmed.' }
        else { Add-Check 'Lightsail' 'pass' 'Coordinator advertises protocol v1. Pairing approval is still required.' }
    }
    if ($Launch) { & (Join-Path $Root 'start-worker.ps1') }
    Write-Host ('Start with: & "{0}"' -f (Join-Path $Root 'start-worker.ps1'))
    exit 0
} catch {
    Add-Check 'Setup' 'failed' $_.Exception.Message
    exit 1
} finally {
    try {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ReportPath) | Out-Null
        Atomic-Json $ReportPath ([ordered]@{ recorded_utc=[DateTime]::UtcNow.ToString('o'); checks=@($Checks.ToArray()) })
        Write-Host "Report: $ReportPath"
    } catch { Write-Warning 'Could not write diagnostic report. The checks are printed above.' }
}
