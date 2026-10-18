$ErrorActionPreference='Stop'
$source=Join-Path $env:LOCALAPPDATA 'Temp\ow-installer-swap-20261018\source'
$fail=0
$p=Join-Path $source 'CMakeLists.txt';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '4636bd0af2c9ba963a0106e721af4958cc8a8875fe4489e587b47b32b6182664'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'config/dependencies.lock.json';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '04802e119423ff97535ab437375bdf33abbb50102aee9c341675d8fde62aab7d'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'config/worker.example.json';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '1d822f102c1341d088e33116f60b09b6ee0ce88d9dfefb19066f2f22a7409e9c'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/adaptive_evidence.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '1f85bdfa525663f5e9d6795bff8b222f7de90ac70e35e42652c3c214e96eafc5'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/adaptive_hardware.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '2d6e252361e3c11c3f2c48a4662a9d1f247cd1d2e9f3fedd6d202967112d51a7'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/admission.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'f5639ddec4302f80b6e5cfdfe72e61ff9879232f304041fe7579f8c606552512'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/capability_report.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'b0fb7d95863b9346ffac2d1e8dc654a4c6f68a8f0325252c5caddfe87b87563b'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/child_memory.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'edb9bb946e206f2dbffa1e5e422a274a4e4ef390ae0b5b409a651db4c5759293'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/console.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'b841a21376f90e6b8af05fdfa55c36bd857104f566f15b7e7b5d7a1098a94a18'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/device_decode.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'bcd492df1e4d8b91c8b88fa458b9d451bd7278e5245372a94dcc83a5893d6a0b'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/device_identity.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '6a60ba20cd398c6e2e8be41e824e09e5b16e29b1f6de7ede3d2f21685db2d847'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/encoder_run.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '4d7f3b737fb9e2d8a5c9410b289b6eb9bd3831b4e38ddc71235c8f142b68f478'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/json.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '4e74418c83b93331fa38dc70ff35f5d8a49c83506c149c05ca8ddd7cf3f95a8e'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/media_renditions.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '0d1ce97caa9bf871da18879306b23218d8a652af475ce2acad9d8f585ba7f34a'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/operator_status.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'f7032b2c053299b6ed1db1c164927ba77486c3186bd48c4908eb955bcea3a3b7'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/pairing.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '8359df015e1f9cafd0c708c9deb09075f711be65f8d25d926d0958b11be5d512'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/probe_retry.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '0dc0b328ac8584ce330f70b7423aa9ae666a9352bf296de986c642951efec751'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/probe_status.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '2705545a98e2e8ab96bb359f948ef71d0a1e1e56627d3614d185de673ca821bf'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/progress.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '84219c2f99b3297fd21ec2c2bea8b2b93ec1fb64398e7f69a558f10b2e0922a6'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/protocol.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '175fd8d20199b7e92aced749852ffdb999b1bff912997c707e528f3795069e48'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/rendition_pipeline.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '9e0c4f1d5f0840b825d3e2649ff1836aafe8085b18e7592d574977adc68f66c7'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/stage_timing.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'd717397f8beafcf361c02ebc526af6babeaa4460c2ec75f7b9c6d407f81a3643'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/store.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'c29577acada62376ec6c0b5748042e3757aa6f3d40177d4d923d3fa1f6d4ccef'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'include/ow/worker_status.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '95f16dc333887e1c4a440e3fcbbb59c195244712f8ed554ee0a786c9bba71dd8'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'scripts/check_ffmpeg_dependency.py';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'bf36275d80601fc3130589b36503d29926235835f84372cf267700e88715dd21'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'scripts/setup-converter.ps1';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '455a2bb517f9851bf7cfdb39cf5e7ddb8bddd7d631ff5fd904734c33c394f87b'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'scripts/setup-dependencies.ps1';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'cf33263dbdc2dfb79b2db81197608ab994c5b602783c9ffb2e1a049f12acfdc9'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'scripts/setup-extract-zip.ps1';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'd5c6623862cbbb42ad338883cb4c73da084146d38c7fc900ab008b65f204b902'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'scripts/setup-identity.ps1';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '7ed7a6dbf738de4094157bc0a4eaefe30e9028b642eacccb3babf53a4ac8cff8'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'scripts/setup-policy.ps1';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '6dac680f60dbed7354fd2f94eef8385e7198655e9bbaa9c4f624b600e980c082'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'scripts/setup-progress.ps1';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '99f312b4f3674b1a506d970af81d7361761c3fec5deea10daaacd1371a3c1962'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'scripts/setup-startup.ps1';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'd62cbce03056813e6c91189c50185898ac631662383f981b277f1ebeb1e237df'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'scripts/setup.ps1';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '992904a454fab563f2f3b5fa5296b80875459cbf2dc1e5a141f901f5f8a78ba4'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'scripts/start-worker.ps1';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '90e1a25c75f1f3d1941e8cf8e929efaca566428d9f6fbd67c17eb481f9318222'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'scripts/test.sh';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '8cb29547147c9bf38fc5594c589248a0f63c199f655c191d8c2c898acd89edbf'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'scripts/update-worker.ps1';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'eef2a712f4697ca54ea1c3a7cbef757a0f94357184a2f75da1852758c82ab168'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'scripts/worker-swap.ps1';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '337261b4678a28719dbc27b816ab8159cde97c4ff02479ef20bef0afe29e245d'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'setup.cmd';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '47fcbad9f3d390fe827f4d4734d3ff360c23ec5f7ab9fa082b4bede431753695'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'src/adaptive_hardware.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'a339b067c21f203287b1931f974129143b556e5da392b40fb924182f1bda64ee'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'src/admission.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'dea37ccf82d48dfaa5bd02f8bc73f712cd34111e7f08e3b9fcc72fe8b26e53e9'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'src/boost_json.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'c629b5d79c7d5dca67425f47c8849120e245e74b8895bf0b3cbf681ffdda9a4d'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'src/pairing.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '52d53b2c03b33e5b80b9e1dd0ec82a3ba18c31cc426ac5e23b1552aa1ed2c99c'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'src/protocol.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'efaa997e95074b83ac3b3f603fd04f2f6293ca0495b7fda22b28755b75c47d66'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'src/store.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'faeaed8f51181e49cec209c3fdf7f1c1bec55e930354a1b9967ebe0352d0fa2e'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/adaptive_evidence_probe.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '0c5641c471e530f70944773973ea248eeb00b83e81ecf8d99f24e4c00dd43000'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/adaptive_hardware_test.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '279e3ebc3fb5e125401ca1a24c3ea982e872673f1ec4ed45e035f297e33a280a'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/adaptive_probe_smoke.py';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '75d7b943229fc997755334e5f71658ad4e48a3573d3d7625d005c33a7e4c9699'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/admission_test.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '8ccdf1a5f0b16c3424417bf5a8653aabf4b4cc9e7c258d6474f04a2c07306c22'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/encoder_run_test.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '30e2dc09429b0b98aed44f3027a23aa74fcaace3b9993a73175b60ec6633023a'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/ffmpeg_dependency_test.py';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'c09dfa17e4ab4abce3e9e899764895e7244767dbbdfdc19510dac0d36634656d'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/main.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'b030b122acb90b798392f37eae354c6d69fd5ed2a3ef5f83c01bd2d572d4f8b0'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/media_rendition_probe.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '4e46acee7f6ee4beb612c10c5d587712058fe7779ccc57cb3df818188bfa20b1'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/media_rendition_smoke.py';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'd409ec5c3280d9567f41878a6b6fb3b8fa998521bce76f130681e50fb85866d9'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/media_rendition_test.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '8b64c52f78da69693905dc00f6e2062602569854fc29e9bc4f62ab2ef6759ff4'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/pairing_test.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'b6ab4892d7caa7addd8a152d8cda912c40025438937a822519cce7cae8759e83'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/pipeline_test.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '51723767dccb9ff24cec72c275aea05ae36e8877673421c29bba11b62cfe73cf'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/progress_test.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '9a24d9e79417d66e73504fdf5fb425b9999792af30be15b6159bc52d27d75dd4'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/protocol_test.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '4ef4c5029d2cd47697203262d70b2a2be95a92c7e4a39e45a4f7029ceae1ca26'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/setup_converter_test.ps1';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '4335737815cc22d4c485c073fb09b49a83f33649c32d0f4ff025b1bf17d33666'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/setup_policy_test.ps1';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'c627aa48736070493ab57b8ace54054698bc03f671a0a9546723e44f9128be7a'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/setup_progress_test.ps1';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'b2045a116a0bcec894c47764cd1301decd23a0f3ffbc75ede3e4bb509896cbe8'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/setup_swap_test.ps1';$b=[IO.File]::ReadAllBytes($p)
$b=[Text.Encoding]::UTF8.GetBytes([Text.Encoding]::UTF8.GetString($b).Replace("`r`n","`n"))
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'f0db3b62e337c1bea566c55a3ad8f1005490a4bcc98e11d2386dccab75ef040d'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/stage_timing_test.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'a647efd6d8994d0d675909c22cf08451b741b7c10dc2cec74eff76dd9fe6b6ea'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/status_test.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '0f15953f5950bb62398101c80643ef50d7a8c5376d89cbb82bce9943f972dd45'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/store_test.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'ade35bf49b89453126c05daa3e93046b6f99e42484cfa769b5b881a8a5d82621'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/test.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '1046491bc5572fe980ee942ca934556b8059222fc9d9faecc01a52d09199c00f'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/windows_child.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'be46c9e0c0d22848c1a3665200875a8b076f9943b9dc39b3fb4ffbe7b5490127'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/windows_gpu_baseline.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'c53d028fb164f9146b0c4c53cbe6b69b9ce522851398f138f696d2b0876adb43'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/windows_gpu_budget_probe.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '7624de46558a69f1ba1be8c7b42314deab3c1e267e74a26228f5639c400b18e6'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/windows_gpu_measurements.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'f73f3eb2e14714035a0a3759e5f11707b0d1326df07802f1ada1599e4dafadbb'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/windows_pipeline_smoke.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'a6783b7254dc36e358797d51f079e2949a4570007088c09618deabae4fc790ef'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'tests/windows_test.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '5775b4dbddda5397d0a0345ccb569b18e88d11bd6b406189bec772289e2c29b1'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/console.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '4ba295060c0ee82929a58422eb80affb123979b5c2491d2d1ad0d0d7d95c2e6a'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/doctor.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '8ac49bcabd99bfa6dc58fe4dda68bf2a913a8f6fd8076ac60538a79e818cdb86'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/hardware.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'ad7117272aedc8b1c679a48b30448fd9d37d0acaea858a246f02ed8c8d725ace'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/hardware.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '33ac684cec1dd5d5453a85f9d18ad49a4aba430585aa5b067e695ded7497c803'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/http.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '92604c9bd8f857603cad9ba6f75b540e53420c7f9c16d0f876432b48a7c72b6a'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/http.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '052f3dacf86ce6a3339333b024f6e220c44b8d5451d1e3c366254d028e15bd5c'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/pairing_adapter.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'e41db29b2b53e644c05fdbff14bdd76f674eb6867f13040661a788924adbf804'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/pairing_adapter.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '317544cc5cfea4c27bbc901988aba9be731726e0e693474075101c8b3ccecf9c'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/physical_adapters.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '68dae65a9643963881ed2135399dbd5a7e2ec2e267451545ba9e956b41f5e394'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/physical_adapters.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '7d755ac7973e1c96cb78c19fde0ff5217fe2b75011a24bad7096657e6fb328a9'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/platform.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '61d411e1bebb9c9242949d0b282b764e9264d2b9e33a6aa50a0c413e77cb3f28'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/platform.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '5817dcfee43e736039f3365e8508bf7ca3614dfc31a784526a437512a39c27dc'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/probe_recovery.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '2d47640c570e4d3b6f5c628d42ab00fc6d45a320c36430f6445f37696828d2e5'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/process.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'b8c68987c62da399a10f6ca740e74b181491e977f57c6bb298b49e04e954a549'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/process.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'bfcd3a73d6fffe6e2cc4e74d10cb0afc7079254ef39c52b3af2a926286e23f89'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/tray.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne 'd363166d4375958cdd170b18c9476f30a00871edb37a19f1becd08daa5114132'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/worker.cpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '3bcc144571719d6f3b0527cdafabaf221997fc6f31619b0f1ef5726bb74a5dba'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
$p=Join-Path $source 'windows/worker.hpp';$b=[IO.File]::ReadAllBytes($p)
$s=[Security.Cryptography.SHA256]::Create();try{$h=([BitConverter]::ToString($s.ComputeHash($b))).Replace('-','').ToLowerInvariant()}finally{$s.Dispose()}
if($h -cne '64741e0211b9251c0af6e2f155c38a4b311331262a41a0012e44b7a6eed3afcc'){Write-Output ('SOURCE_MISMATCH='+$p);$fail++}
Write-Output ('SOURCE_FILES_VERIFIED=90 FAILURES='+$fail);if($fail){exit 1};exit 0
