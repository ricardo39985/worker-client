# Shared update boundaries. No worker is ever killed to make an update pass.
function Get-WorkerCanonicalJson($Value) {
    function Canonical($v) {
        if ($null -eq $v) { return $null }
        if ($v -is [System.Collections.IDictionary]) { $o=[ordered]@{};foreach($k in @($v.Keys|Sort-Object)){$o[$k]=Canonical $v[$k]};return $o }
        if ($v -is [pscustomobject]) { $o=[ordered]@{};foreach($p in @($v.PSObject.Properties|Sort-Object Name)){$o[$p.Name]=Canonical $p.Value};return $o }
        if ($v -is [array]) { $a=New-Object 'Collections.Generic.List[object]';foreach($item in $v){$a.Add((Canonical $item))};return ,($a.ToArray()) }
        return $v
    }
    $c=Canonical $Value
    return ConvertTo-Json -InputObject $c -Depth 32 -Compress
}
function Test-WorkerRetainedConfiguration($Before,$After,$ExpectedOperator,[bool]$ColdOnly=$false) {
    $a=[ordered]@{};$b=[ordered]@{}
    foreach($p in $Before.PSObject.Properties){if($p.Name -ne 'operator'){$a[$p.Name]=$p.Value}}
    foreach($p in $After.PSObject.Properties){if($p.Name -ne 'operator'){$b[$p.Name]=$p.Value}}
    if((Get-WorkerCanonicalJson $a) -cne (Get-WorkerCanonicalJson $b)){throw 'Existing worker settings/limits changed during update'}
    if($ColdOnly){return}
    if($ExpectedOperator) {
        $op=$After.PSObject.Properties['operator']
        if(-not $op){throw 'Migrated operator preferences missing'}
        $visible=[ordered]@{};foreach($p in $ExpectedOperator.PSObject.Properties){$actual=$op.Value.PSObject.Properties[$p.Name];if(-not $actual){throw 'Migrated operator preference missing'};$visible[$p.Name]=$actual.Value}
        if((Get-WorkerCanonicalJson $ExpectedOperator) -cne (Get-WorkerCanonicalJson $visible)){throw 'Migrated operator preferences differ from preserved intent'}
        $oldOpaque=[ordered]@{};$newOpaque=[ordered]@{}
        if($Before.PSObject.Properties['operator']){foreach($p in $Before.operator.PSObject.Properties){if(-not $ExpectedOperator.PSObject.Properties[$p.Name]){$oldOpaque[$p.Name]=$p.Value}}}
        foreach($p in $op.Value.PSObject.Properties){if(-not $ExpectedOperator.PSObject.Properties[$p.Name]){$newOpaque[$p.Name]=$p.Value}}
        if((Get-WorkerCanonicalJson $oldOpaque) -cne (Get-WorkerCanonicalJson $newOpaque)){throw 'Opaque operator metadata changed'}
    } elseif($Before.PSObject.Properties['operator']) {
        if(-not $After.PSObject.Properties['operator'] -or (Get-WorkerCanonicalJson $Before.operator) -cne (Get-WorkerCanonicalJson $After.operator)){throw 'Existing operator preferences changed'}
    }
}
function Assert-WorkerCooperativeExitAvailable {
    param([string]$Root,[object[]]$Workers)
    $allowed=[IO.Path]::GetFullPath((Join-Path $Root 'versions'))+[IO.Path]::DirectorySeparatorChar
    foreach($p in $Workers) {
        if(-not $p.Path -or -not $p.Path.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected worker image; update refused'}
        $ctl=Join-Path (Split-Path $p.Path) 'OrganizerWorkerCtl.exe'
        if(-not (Test-Path -LiteralPath $ctl)) {
            $e=New-Object System.InvalidOperationException('OWNER_EXIT_REQUIRED: old tray has no authenticated CLI. Choose tray Exit / finish current jobs, wait for it to exit, then retry. No worker was terminated.')
            $e.Data['worker_error']='owner_exit_required';throw $e
        }
    }
}
function Invoke-WorkerCooperativeStop {
    param([string]$Root,[scriptblock]$FindWorkers,[scriptblock]$RequestDrain,[int]$WaitSeconds=600)
    if($WaitSeconds -lt 0 -or $WaitSeconds -gt 1800){throw 'Cooperative wait must be 0..1800 seconds'}
    $workers=@(& $FindWorkers)
    Assert-WorkerCooperativeExitAvailable -Root $Root -Workers $workers
    foreach($p in $workers) {
        $ctl=Join-Path (Split-Path $p.Path) 'OrganizerWorkerCtl.exe'
        & $RequestDrain $ctl
    }
    $until=[DateTime]::UtcNow.AddSeconds($WaitSeconds)
    while(@(& $FindWorkers).Count) {
        if([DateTime]::UtcNow -ge $until){throw 'DRAIN_PENDING: existing jobs/controller still running; no process was killed'}
        Start-Sleep -Milliseconds 200
    }
}
