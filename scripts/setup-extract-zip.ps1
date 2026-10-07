#requires -Version 5.1
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Archive,[Parameter(Mandatory=$true)][string]$Destination)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
# Run the built-in extractor in an owned process. Parent setup can keep printing
# elapsed/quiet status, enforce a timeout, and retain checksum-verified input.
try {Expand-Archive -LiteralPath $Archive -DestinationPath $Destination;exit 0}
catch {Write-Error $_ -ErrorAction Continue;exit 1}
