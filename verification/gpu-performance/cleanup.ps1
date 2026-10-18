$ErrorActionPreference='Stop';$root='C:\Users\danny\AppData\Local\Temp\ow-gpu-measured-20261018-a72c'
if($root -ne 'C:\Users\danny\AppData\Local\Temp\ow-gpu-measured-20261018-a72c'){throw 'Unexpected cleanup target'}
foreach($name in @('ow_gpu_measurements','ow_windows_tests','ow_test_child','ow_contract_tests','ow_pipeline_contract_tests','ow_rendition_contract_tests','ow_progress_contract_tests','ow_adaptive_hardware_contract_tests')) {
 foreach($process in [Diagnostics.Process]::GetProcessesByName($name)) {
  if($process.MainModule.FileName.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Owned test process is still running; refusing cleanup'}
 }
}
[IO.Directory]::Delete($root,$true)
Write-Output ('REMOVED_ROOT='+$root)
Write-Output ('ROOT_EXISTS='+[IO.Directory]::Exists($root))
if([IO.Directory]::Exists($root)){throw 'Directory still exists'}
