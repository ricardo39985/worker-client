function Invoke-WorkerRuntimeBootstrap([scriptblock]$Present,[scriptblock]$Approve,[scriptblock]$Prepare,[scriptblock]$Install) {
    if(& $Present){return}
    if(-not (& $Approve)){throw 'CPU runtime bootstrap requires explicit Microsoft runtime license approval.'}
    & $Prepare
    & $Install
    if(-not (& $Present)){throw 'Microsoft runtime installation did not supply the required DLLs; inference cannot be advertised.'}
}
function Ensure-WorkerInferenceRuntime {
    $vc=$lock.embeddinggemma2.vc_runtime
    $installer=Join-Path $cache ('vc-runtime-'+$vc.sha256+'.exe')
    Invoke-WorkerRuntimeBootstrap -Present {
        $system=Join-Path $env:SystemRoot 'System32'
        return @(@('MSVCP140.dll','VCRUNTIME140.dll','VCRUNTIME140_1.dll') | Where-Object {-not (Test-Path -LiteralPath (Join-Path $system $_) -PathType Leaf)}).Count -eq 0
    } -Approve {
        if($AcceptRuntimeLicense){return $true}
        if($NonInteractive){throw 'Missing Microsoft C++ runtime: review its license and supply -AcceptRuntimeLicense, or -NoInference.'}
        Write-Host ('CPU inference requires Microsoft Visual C++ Redistributable '+$vc.version+'. Installation may request UAC; the worker still runs as your normal user.')
        Write-Host $vc.url
        return (Read-WorkerSetupInput 'Type INSTALL to accept the Microsoft runtime license and install this pinned dependency, or Enter to cancel') -ceq 'INSTALL'
    } -Prepare {
        Fetch-Verified $vc.url $installer $vc.sha256
        $signature=Invoke-WorkerSetupStage 'Verify Microsoft CPU runtime installer signature' {Get-AuthenticodeSignature -LiteralPath $installer}
        if($signature.Status -ne 'Valid' -or -not $signature.SignerCertificate -or $signature.SignerCertificate.Subject -notmatch '(^|,\s*)O=Microsoft Corporation(,|$)'){throw 'CPU runtime installer is not validly signed by Microsoft Corporation; it will not run.'}
    } -Install {
        Write-WorkerSetupLine '[WAITING FOR INPUT] Approve Windows UAC for the Microsoft runtime only.'
        $process=Start-Process -FilePath $installer -Verb RunAs -ArgumentList @('/install','/passive','/norestart') -PassThru
        $code=Wait-WorkerSetupProcess $process 'Install Microsoft CPU runtime' -TimeoutSeconds 900
        if($code -eq 3010){throw 'Microsoft runtime installed; reboot Windows yourself and rerun setup.'}
        if($code -ne 0){throw ('Microsoft runtime installation failed or was cancelled (exit '+$code+').')}
    }
}
# Per-user, pinned native CPU inference. No global service, Python or GPU required.
function Configure-WorkerEmbedding {
    param([switch]$Force)
    $e=$lock.embeddinggemma2
    if (-not $EnableInference -and -not ($config.PSObject.Properties.Name -contains 'embedding')) { return }
    if ($NoInference) { if($config.PSObject.Properties.Name -contains 'embedding'){$config.PSObject.Properties.Remove('embedding')};return }
    Ensure-WorkerInferenceRuntime
    $zip=Join-Path $cache ('llama-'+$e.runtime.sha256+'.zip')
    Fetch-Verified $e.runtime.url $zip $e.runtime.sha256
    $runtime=Join-Path $dependencies ('llama-'+$e.runtime.sha256.Substring(0,16))
    if($Force -and (Test-Path -LiteralPath $runtime)){Remove-WorkerGeneratedTree $runtime}
    Expand-WorkerDependency -Directory $runtime -ArchiveSha256 $e.runtime.sha256 -Extract {
        param($stage)
        Native (Join-Path $PSHOME 'powershell.exe') @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'setup-extract-zip.ps1'),'-Archive',$zip,'-Destination',$stage) -Name 'Extract native CPU model runtime' -TimeoutSeconds 120
    }
    $server=@(Get-ChildItem -LiteralPath $runtime -Filter 'llama-server.exe' -Recurse)
    if($server.Count -ne 1){throw 'Pinned CPU runtime does not contain one llama-server.exe.'}
    $files=@(Get-ChildItem -LiteralPath $server[0].DirectoryName -File | Where-Object {$_.Extension -in '.exe','.dll'} | ForEach-Object {@{path=$_.FullName;sha256=(Get-WorkerFileSha256 $_.FullName -Quiet)}})
    $modelDir=Join-Path $Root 'models';New-Item -ItemType Directory -Force -Path $modelDir | Out-Null
    foreach($file in @($e.model,$e.projector)){Fetch-Verified $file.url (Join-Path $modelDir $file.name) $file.sha256}
    $value=@{profile=$e.profile;server_path=$server[0].FullName;model_path=(Join-Path $modelDir $e.model.name);projector_path=(Join-Path $modelDir $e.projector.name);runtime_files=$files}
    $config | Add-Member -MemberType NoteProperty -Name embedding -Value $value -Force
    Add-Check 'CPU inference' 'info' 'Pinned EmbeddingGemma 2 installed. Text/media functional probes must pass before capabilities are advertised.'
}
