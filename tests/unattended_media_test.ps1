param([Parameter(Mandatory=$true)][string]$BinaryDirectory,[Parameter(Mandatory=$true)][string]$ApprovedFFmpeg,[Parameter(Mandatory=$true)][string]$ApprovedSha256)
$ErrorActionPreference='Stop'
if((Get-FileHash -LiteralPath $ApprovedFFmpeg -Algorithm SHA256).Hash -ine $ApprovedSha256){throw 'Converter must match the already approved dependency lock'}
$root=Join-Path ([IO.Path]::GetTempPath()) ('ow-unattended-media-'+[guid]::NewGuid().ToString('N'))
& (Join-Path $BinaryDirectory 'ow_limited_fixture_runner.exe') $root $ApprovedFFmpeg $ApprovedSha256
if($LASTEXITCODE){throw 'Actual limited-owner unattended fixture failed; no skipped context/DPAPI/child gates'}
$r=Get-Content (Join-Path $root 'acceptance-result.json') -Raw|ConvertFrom-Json
if(-not $r.fixture -or $r.real_coordinator -or -not $r.non_elevated -or -not $r.actual_typed_child -or -not $r.headless_existing_owner_dpapi -or -not $r.matching_ack_permitted_exit -or $r.outbox_transmissions -lt 2){throw 'Unattended media acceptance incomplete'}
Write-Output 'UNATTENDED_MEDIA_PASS synthetic coordinator, actual restricted owner/DPAPI/typed child/IPC/leases/retry/ACK; NOT a cold boot or production job'
$r|ConvertTo-Json -Compress
# Keep owned failed/success test data for runner diagnostics; never remove another installation.
