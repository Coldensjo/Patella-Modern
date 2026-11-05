# Moves the decompilation workspace (the analysis of Tibia.exe: raw and clean pseudocode, REA output, the
# name/symbol/range databases, the analysis notes and tools) between its working places in the checkout and
# decompilation\, which keeps the same relative paths. Both places are gitignored; the files are not part of
# the repository since November 2, 2026 (the last commit that has them: see decompilation\MANIFEST.txt).
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\decompilation.ps1 -Restore   # back to work on it
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\decompilation.ps1 -Stash     # out of the way again
#
# Never overwrites: a path that exists at the destination stops the script before anything moves.

param(
	[switch]$Restore,
	[switch]$Stash
)

$ErrorActionPreference = 'Stop'
if ($Restore -eq $Stash) {
	Write-Error 'decompilation.ps1: give -Restore or -Stash'
}

$Root = Split-Path -Parent $PSScriptRoot
$Store = Join-Path $Root 'decompilation'

# Relative paths (files and folders), the same in both places.
$Paths = @(
	'decomp',
	'rea-out',
	'names.tsv',
	'symbols.tsv',
	'file-ranges.tsv',
	'notes\source-map.md',
	'notes\findings.md',
	'notes\Communication.md',
	'notes\Talk.md',
	'tools\triage.py',
	'tools\simplify.py',
	'tools\callcheck.py',
	'tools\ehinfo.py',
	'tools\ehcompare.py',
	'tools\xctable.py',
	'tools\listing.cmd'
)

if ($Restore) { $From = $Store; $To = $Root } else { $From = $Root; $To = $Store }

$Moves = @()
foreach ($Path in $Paths) {
	$Source = Join-Path $From $Path
	$Target = Join-Path $To $Path
	if (-not (Test-Path -LiteralPath $Source)) {
		continue
	}
	if (Test-Path -LiteralPath $Target) {
		Write-Error "decompilation.ps1: $Target exists already; nothing was moved"
	}
	$Moves += ,@($Source, $Target)
}
if ($Moves.Count -eq 0) {
	Write-Output 'decompilation.ps1: nothing to move'
	exit 0
}
foreach ($Move in $Moves) {
	$Parent = Split-Path -Parent $Move[1]
	if (-not (Test-Path -LiteralPath $Parent)) {
		New-Item -ItemType Directory -Force $Parent | Out-Null
	}
	Move-Item -LiteralPath $Move[0] -Destination $Move[1]
	Write-Output ("moved {0} -> {1}" -f $Move[0], $Move[1])
}
