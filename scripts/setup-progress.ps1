#requires -Version 5.1
# Dot-sourcing defines functions only; it does not start work or write files.
if(-not (Get-Variable WorkerSetupProgressLog -Scope Script -ErrorAction SilentlyContinue)){$script:WorkerSetupProgressLog=$null}
function Get-WorkerProgressClock { return [long]([Diagnostics.Stopwatch]::GetTimestamp()*1000.0/[Diagnostics.Stopwatch]::Frequency) }
function Protect-WorkerSetupOutput([string]$Text) {
    $safe=[regex]::Replace($Text,'(?i)https?://[^\s"<>]+','[URL]')
    $safe=[regex]::Replace($safe,'(?i)Authorization\s*[:=][^\r\n]*','Authorization: [REDACTED]')
    $safe=[regex]::Replace($safe,'(?i)(\bBearer\s+)[^\s,"''}]+','$1[REDACTED]')
    return [regex]::Replace($safe,'(?i)(["'']?\b(?:[a-z_]*token|secret|password|credential|private_key|device_code|challenge)["'']?\s*[:=]\s*)(?:"[^"]*"|''[^'']*''|[^\s,;}]+)','$1[REDACTED]')
}
function Write-WorkerSetupLine([string]$Text) {
    $safe=Protect-WorkerSetupOutput $Text
    Write-Host $safe
    if($script:WorkerSetupProgressLog){
        try {
            if((Test-Path -LiteralPath $script:WorkerSetupProgressLog) -and (Get-Item -LiteralPath $script:WorkerSetupProgressLog).Length -ge 5MB){
                [IO.File]::Copy($script:WorkerSetupProgressLog,$script:WorkerSetupProgressLog+'.previous',$true)
                [IO.File]::WriteAllText($script:WorkerSetupProgressLog,'',[Text.UTF8Encoding]::new($false))
            }
            [IO.File]::AppendAllText($script:WorkerSetupProgressLog,$safe+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
        } catch { Write-Warning 'Progress log is unavailable; terminal progress remains active.';$script:WorkerSetupProgressLog=$null }
    }
}
function Write-WorkerProgressEvent($Event) {
    $work=''
    if($Event.unit -eq 'bytes'){$work=('{0:N1}' -f ($Event.completed/1MB))+' MiB';if($Event.total -gt 0){$work+=' / '+('{0:N1}' -f ($Event.total/1MB))+' MiB'}}
    elseif($Event.completed -gt 0 -or $Event.total -gt 0){$work=[string]$Event.completed;if($Event.total -gt 0){$work+=' / '+$Event.total};$work+=' '+$Event.unit}
    if($null -ne $Event.percent){$work+=' ('+$Event.percent+'%)'}
    $detail=$Event.detail
    if($Event.status -eq 'quiet'){$detail='No measured progress for '+[math]::Floor($Event.quiet_seconds)+'s; work may be waiting on I/O. '+$detail}
    Write-WorkerSetupLine ('[{0}] [{1}] {2} | {3} | elapsed {4:N1}s | {5}' -f ([DateTime]::Now.ToString('HH:mm:ss')),$Event.status.ToUpperInvariant(),$Event.name,$work,$Event.elapsed_seconds,$detail)
}
function New-WorkerProgress([string]$Name,[long]$Total=0,[string]$Unit='units',[int]$QuietSeconds=30,[scriptblock]$Clock={Get-WorkerProgressClock},[scriptblock]$Emit={param($e)Write-WorkerProgressEvent $e}) {
    $now=& $Clock
    $p=[pscustomobject]@{Name=$Name;Total=$Total;Unit=$Unit;Completed=[long]0;StartedMs=[long]$now;LastActivityMs=[long]$now;LastEmitMs=[long]$now;QuietSeconds=$QuietSeconds;Clock=$Clock;Emit=$Emit;Finished=$false}
    Send-WorkerProgressEvent $p 'started' ''
    return $p
}
function Send-WorkerProgressEvent($Progress,[string]$Status,[string]$Detail) {
    $now=[long](& $Progress.Clock)
    $percent=$null
    if($Progress.Total -gt 0){$percent=[math]::Min(100,[math]::Floor(100.0*$Progress.Completed/$Progress.Total))}
    $event=[pscustomobject]@{name=$Progress.Name;status=$Status;completed=$Progress.Completed;total=$Progress.Total;unit=$Progress.Unit;percent=$percent;elapsed_seconds=($now-$Progress.StartedMs)/1000.0;quiet_seconds=($now-$Progress.LastActivityMs)/1000.0;detail=$Detail}
    & $Progress.Emit $event | Out-Null
    $Progress.LastEmitMs=$now
}
function Update-WorkerProgress($Progress,[long]$Completed=-1,[string]$Detail='',[switch]$Force) {
    if($Progress.Finished){return}
    $now=[long](& $Progress.Clock)
    if($Completed -gt $Progress.Completed){$Progress.Completed=$Completed;$Progress.LastActivityMs=$now}
    if($Force -or $now-$Progress.LastEmitMs -ge 2000){
        $status=if($now-$Progress.LastActivityMs -ge $Progress.QuietSeconds*1000){'quiet'}else{'progress'}
        Send-WorkerProgressEvent $Progress $status $Detail
    }
}
function Complete-WorkerProgress($Progress,[switch]$Failed,[string]$Detail='') {
    if($Progress.Finished){return}
    if(-not $Failed -and $Progress.Total -gt 0){$Progress.Completed=$Progress.Total}
    Send-WorkerProgressEvent $Progress $(if($Failed){'failed'}else{'done'}) $Detail
    $Progress.Finished=$true
}
function Invoke-WorkerSetupStage([string]$Name,[scriptblock]$Operation) {
    $p=New-WorkerProgress $Name
    try {$result=& $Operation;Complete-WorkerProgress $p;return $result}
    catch {Complete-WorkerProgress $p -Failed -Detail $_.Exception.Message;throw}
}
function Read-WorkerSetupInput([string]$Prompt) {
    Write-WorkerSetupLine ('[WAITING FOR INPUT] '+$Prompt)
    return (Read-Host $Prompt)
}
function Get-WorkerFileSha256([string]$Path,[switch]$Quiet) {
    $p=$null;$stream=$null;$sha=$null
    try {
        $stream=[IO.File]::OpenRead($Path);$size=$stream.Length
        if(-not $Quiet){$p=New-WorkerProgress ('SHA-256 '+[IO.Path]::GetFileName($Path)) -Total $size -Unit bytes}
        $sha=[Security.Cryptography.SHA256]::Create();$buffer=New-Object byte[] (1MB);$done=[long]0
        while(($count=$stream.Read($buffer,0,$buffer.Length)) -gt 0){
            $null=$sha.TransformBlock($buffer,0,$count,$buffer,0);$done+=$count
            if($p){Update-WorkerProgress $p -Completed $done}
        }
        $null=$sha.TransformFinalBlock($buffer,0,0)
        if($done -ne $size){throw 'File size changed during checksum verification.'}
        if($p){Complete-WorkerProgress $p}
        return [BitConverter]::ToString($sha.Hash).Replace('-','').ToLowerInvariant()
    } catch {if($p){Complete-WorkerProgress $p -Failed -Detail $_.Exception.Message};throw}
    finally {if($stream){$stream.Dispose()};if($sha){$sha.Dispose()}}
}
function Receive-WorkerStream([IO.Stream]$Stream,[string]$Destination,[long]$Length,$Progress,[long]$MaximumBytes=1GB,[int]$IdleSeconds=90,[int]$TimeoutSeconds=1800) {
    if($Length -gt $MaximumBytes){throw 'Download exceeds the dependency byte limit.'}
    $Progress.Unit='bytes';$Progress.Total=[math]::Max(0,$Length)
    $out=$null;$cancel=[Threading.CancellationTokenSource]::new()
    try {
        $out=[IO.File]::Open($Destination,[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::Read)
        $buffer=New-Object byte[] 65536;$done=[long]0
        while($true){
            if(([long](& $Progress.Clock))-$Progress.StartedMs -ge $TimeoutSeconds*1000){throw 'Dependency download exceeded its overall timeout.'}
            $read=$Stream.ReadAsync($buffer,0,$buffer.Length,$cancel.Token)
            while(-not $read.IsCompleted){
                $now=[long](& $Progress.Clock)
                Update-WorkerProgress $Progress -Completed $done -Detail 'Waiting for the next data block'
                if($now-$Progress.LastActivityMs -ge $IdleSeconds*1000){throw 'No download bytes received for the idle timeout; transfer stopped for retry.'}
                if($now-$Progress.StartedMs -ge $TimeoutSeconds*1000){throw 'Dependency download exceeded its overall timeout.'}
                Start-Sleep -Milliseconds 100
            }
            $count=$read.GetAwaiter().GetResult()
            if($count -eq 0){break}
            $done+=$count
            if($done -gt $MaximumBytes -or ($Length -ge 0 -and $done -gt $Length)){throw 'Download size exceeds the declared bound.'}
            $out.Write($buffer,0,$count);Update-WorkerProgress $Progress -Completed $done
        }
        if($Length -ge 0 -and $done -ne $Length){throw 'Download was truncated.'}
        $out.Flush()
    } finally {$cancel.Cancel();$cancel.Dispose();if($out){$out.Dispose()}}
}
function Get-WorkerSetupDownload([string]$Url,[string]$Destination,[string]$Name='Downloading dependency') {
    $p=New-WorkerProgress $Name -Unit bytes
    $client=$null;$response=$null;$stream=$null;$request=$null
    try {
        Add-Type -AssemblyName System.Net.Http
        $handler=[Net.Http.HttpClientHandler]::new();$handler.AllowAutoRedirect=$false;$handler.UseCookies=$false;$handler.UseDefaultCredentials=$false
        $client=[Net.Http.HttpClient]::new($handler);$client.Timeout=[TimeSpan]::FromMinutes(30)
        $current=[uri]$Url
        for($redirect=0;$redirect -le 5;$redirect++){
            if($current.Scheme -ne 'https' -or $current.UserInfo -or $current.Fragment){throw 'Dependency downloads require HTTPS without credentials or fragments.'}
            $request=[Net.Http.HttpRequestMessage]::new([Net.Http.HttpMethod]::Get,$current)
            $task=$client.SendAsync($request,[Net.Http.HttpCompletionOption]::ResponseHeadersRead)
            $headerStart=Get-WorkerProgressClock
            while(-not $task.IsCompleted){
                Update-WorkerProgress $p -Detail 'Connecting; no response headers yet'
                if((Get-WorkerProgressClock)-$headerStart -ge 90000){throw 'Download connection timed out before response headers.'}
                Start-Sleep -Milliseconds 100
            }
            $response=$task.GetAwaiter().GetResult();$code=[int]$response.StatusCode
            if($code -in @(301,302,303,307,308)){
                if($redirect -eq 5 -or -not $response.Headers.Location){throw 'Dependency redirect limit exceeded.'}
                $current=[uri]::new($current,$response.Headers.Location.OriginalString)
                $response.Dispose();$response=$null;$request.Dispose();$request=$null;continue
            }
            if($code -ne 200){throw ('Dependency server returned HTTP '+$code)}
            $stream=$response.Content.ReadAsStreamAsync().GetAwaiter().GetResult()
            $length=if($null -eq $response.Content.Headers.ContentLength){[long]-1}else{[long]$response.Content.Headers.ContentLength}
            # Response headers are activity, not downloaded bytes.
            $p.LastActivityMs=Get-WorkerProgressClock
            Receive-WorkerStream $stream $Destination $length $p
            Complete-WorkerProgress $p -Detail 'Downloaded; checksum verification follows'
            return
        }
    } catch {Complete-WorkerProgress $p -Failed -Detail $_.Exception.Message;throw}
    finally {if($client){$client.CancelPendingRequests()};if($stream){$stream.Dispose()};if($response){$response.Dispose()};if($request){$request.Dispose()};if($client){$client.Dispose()}}
}
function ConvertTo-WorkerNativeArgument([string]$Value) {
    # Microsoft CRT quoting, including empty values and backslashes before quotes.
    $result=[Text.StringBuilder]::new();$null=$result.Append('"');$slashes=0
    foreach($c in $Value.ToCharArray()){
        if($c -eq '\'){$slashes++;continue}
        if($c -eq '"'){$null=$result.Append(('\'*(2*$slashes+1))+'"')}
        else {$null=$result.Append(('\'*$slashes)+$c)}
        $slashes=0
    }
    $null=$result.Append(('\'*(2*$slashes))+'"');return $result.ToString()
}
function Stop-WorkerSetupProcess([Diagnostics.Process]$Process) {
    if($Process.HasExited){return}
    # Only the process launched for this setup stage and its descendants.
    $kill=Join-Path $env:SystemRoot 'System32\taskkill.exe'
    $previous=$ErrorActionPreference
    try {$ErrorActionPreference='Continue';& $kill /PID ([string]$Process.Id) /T /F 2>&1 | Out-Null}
    finally {$ErrorActionPreference=$previous}
    if(-not $Process.WaitForExit(5000)){throw 'Timed-out setup process did not terminate.'}
}
function Invoke-WorkerNativeProgress([string]$File,[string[]]$Arguments,[string]$Name='Native command',[int]$TimeoutSeconds=1800,[switch]$CaptureOutput,[switch]$PassExitCode,[int[]]$AcceptExitCodes=@(0),[scriptblock]$OnLine) {
    if(-not (Test-Path -LiteralPath $File -PathType Leaf)){throw ('Required executable missing for stage '+$Name)}
    $p=New-WorkerProgress $Name -Unit 'output lines';$process=[Diagnostics.Process]::new();$capture=[Text.StringBuilder]::new();$started=$false
    try {
        $info=[Diagnostics.ProcessStartInfo]::new();$info.FileName=$File
        $info.Arguments=(@($Arguments | ForEach-Object {ConvertTo-WorkerNativeArgument $_}) -join ' ')
        $info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
        $info.WorkingDirectory=(Get-Location).ProviderPath;$process.StartInfo=$info
        if(-not $process.Start()){throw 'Could not start the native setup process.'}
        $started=$true
        $readers=@($process.StandardOutput,$process.StandardError)
        $tasks=@(($readers[0].ReadLineAsync()),($readers[1].ReadLineAsync()));$ended=@($false,$false);$lines=[long]0
        while(-not ($process.HasExited -and $ended[0] -and $ended[1])){
            for($i=0;$i -lt 2;$i++){
                for($batch=0;$batch -lt 128 -and -not $ended[$i] -and $tasks[$i].IsCompleted;$batch++){
                    $line=$tasks[$i].GetAwaiter().GetResult()
                    if($null -eq $line){$ended[$i]=$true;break}
                    $lines++;Update-WorkerProgress $p -Completed $lines
                    if($OnLine){& $OnLine $line ($i -eq 1) | Out-Null}
                    if($CaptureOutput -and $i -eq 0){
                        if($capture.Length+$line.Length -gt 4MB){throw 'Native output exceeded the capture limit.'}
                        $null=$capture.AppendLine($line)
                    } else {Write-WorkerSetupLine $line}
                    $tasks[$i]=$readers[$i].ReadLineAsync()
                }
            }
            Update-WorkerProgress $p -Completed $lines -Detail 'Process running; percentage unavailable'
            if((Get-WorkerProgressClock)-$p.StartedMs -ge $TimeoutSeconds*1000){throw ('Stage exceeded its '+$TimeoutSeconds+'s timeout.')}
            Start-Sleep -Milliseconds 100
        }
        $process.WaitForExit();$code=$process.ExitCode
        if($code -notin $AcceptExitCodes){throw ('Native stage failed with exit code '+$code)}
        Complete-WorkerProgress $p
        if($CaptureOutput){return $capture.ToString()}
        if($PassExitCode){return $code}
    } catch {Complete-WorkerProgress $p -Failed -Detail $_.Exception.Message;throw}
    finally {try {if($started -and -not $process.HasExited){Stop-WorkerSetupProcess $process}} finally {$process.Dispose()}}
}
function Wait-WorkerSetupProcess([Diagnostics.Process]$Process,[string]$Name,[int]$TimeoutSeconds=7200) {
    # GUI installers and UAC helpers have no readable progress pipe. Do not invent a percentage.
    $p=New-WorkerProgress $Name
    try {
        while(-not $Process.WaitForExit(100)){
            Update-WorkerProgress $p -Detail 'Check the installer/UAC window; percentage unavailable'
            if((Get-WorkerProgressClock)-$p.StartedMs -ge $TimeoutSeconds*1000){
                # Do not forcibly terminate an elevated system installer or interrupt its transaction.
                throw 'Installer wait timed out. The installer may still be running; check it before rerunning setup.'
            }
        }
        $Process.Refresh();$code=$Process.ExitCode
        Complete-WorkerProgress $p -Failed:($code -ne 0) -Detail ('Installer returned exit code '+$code)
        return $code
    } catch {Complete-WorkerProgress $p -Failed -Detail $_.Exception.Message;throw}
    finally {$Process.Dispose()}
}
