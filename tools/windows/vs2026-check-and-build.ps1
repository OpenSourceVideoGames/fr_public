[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Release',

    [ValidateSet('Win32','x64')]
    [string]$Platform = 'Win32',

    [switch]$Build,

    [switch]$IncludeLegacySln,

    [string]$ForcePlatformToolset = 'v143'
)

$ErrorActionPreference = 'Stop'

function Info([string]$msg) { Write-Host "[INFO] $msg" -ForegroundColor Cyan }
function Warn([string]$msg) { Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Ok([string]$msg) { Write-Host "[ OK ] $msg" -ForegroundColor Green }
function Fail([string]$msg) { Write-Host "[FAIL] $msg" -ForegroundColor Red }

$scriptDir = Split-Path -Parent $PSCommandPath
$repoRoot = Resolve-Path (Join-Path $scriptDir '..\..')

Info "Repo root: $repoRoot"

$solutions = Get-ChildItem -Path $repoRoot -Recurse -Filter *.sln | Sort-Object FullName
if (-not $solutions) {
    Fail 'No .sln files found.'
    exit 1
}

$solutionMeta = foreach ($sln in $solutions) {
    $text = Get-Content -Raw -Path $sln.FullName
    $hasLegacyVcproj = $text -match '\.vcproj"'

    $kind = if ($hasLegacyVcproj) { 'legacy-vcproj' } else { 'vcxproj' }

    [pscustomobject]@{
        Path = $sln.FullName
        Kind = $kind
    }
}

Info "Found $($solutionMeta.Count) solutions"
$solutionMeta | ForEach-Object {
    Write-Host ("  - {0} [{1}]" -f ($_.Path.Substring($repoRoot.Path.Length + 1)), $_.Kind)
}

# Tool checks
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    Fail "vswhere not found: $vswhere"
    Warn 'Install Visual Studio 2026 (or 2022/2025+) with C++ workload first.'
    exit 1
}

$vsInstallPath = & $vswhere -latest -requires Microsoft.Component.MSBuild -property installationPath
if (-not $vsInstallPath) {
    Fail 'Visual Studio with MSBuild component not found.'
    exit 1
}

$vsInstallPath = $vsInstallPath.Trim()
$vsDevCmd = Join-Path $vsInstallPath 'Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path $vsDevCmd)) {
    Fail "VsDevCmd.bat not found: $vsDevCmd"
    exit 1
}
Ok "Visual Studio detected: $vsInstallPath"

$requiredCommands = @('yasm', 'nasm')
foreach ($cmd in $requiredCommands) {
    if (Get-Command $cmd -ErrorAction SilentlyContinue) {
        Ok "$cmd found in PATH"
    } else {
        Warn "$cmd not found in PATH"
    }
}

if (-not (Get-Command yasm -ErrorAction SilentlyContinue)) {
    Warn 'Projects that need YASM will fail: kkrunchy, genthree, RG2, werkkzeug3, v2 parts.'
}
if (-not (Get-Command nasm -ErrorAction SilentlyContinue)) {
    Warn 'Projects that need NASM may fail (notably v2 plugin variants and kkrunchy_k7 model_asm).'
} else {
    $nasmFormats = (& nasm -hf 2>$null | Out-String)
    $usesNasmRdf = $false
    $k7proj = Join-Path $repoRoot 'kkrunchy_k7\kkrunchy.vcxproj'
    if (Test-Path $k7proj) {
        $usesNasmRdf = (Get-Content -Raw $k7proj) -match 'nasm\s+-f\s+rdf'
    }
    if ($usesNasmRdf -and $nasmFormats -notmatch '\brdf\b') {
        Warn 'kkrunchy_k7 is configured to use NASM RDF output, but installed NASM has no rdf format.'
    } elseif ($nasmFormats -match '\brdf\b') {
        Ok 'NASM supports rdf output format'
    } else {
        Info 'NASM rdf format not available; current kkrunchy_k7 config does not require nasm rdf.'
    }
}

$placeholders = @(
    'v2\in_v2m\PLACE_WINAMP_SDK_HEADERS_HERE',
    'v2\vsti\PLACE_BUZZ_HEADERS_HERE',
    'v2\vsti\vst2\PLACE_VST2_SDK_HERE',
    'v2\vsti\wtl\PLACE_WTL_HERE'
)

foreach ($ph in $placeholders) {
    $p = Join-Path $repoRoot $ph
    if (Test-Path $p) {
        Warn "Placeholder still present: $ph"
    }
}

$rg2WtlInclude = Join-Path $repoRoot 'WTL\include'
if (-not (Test-Path $rg2WtlInclude)) {
    Warn 'RG2 expects external WTL include path at ..\\..\\WTL\\include from RG2 folder.'
}

if (-not $env:DXSDK_DIR) {
    Warn 'DXSDK_DIR is not set. Some Altona/legacy DX9 tooling may fail (d3dx9 dependency).'
} else {
    Ok "DXSDK_DIR=$($env:DXSDK_DIR)"
}

if (-not $Build) {
    Info 'Build switch not provided. Check phase complete.'
    exit 0
}

if ($Platform -ne 'Win32') {
    Warn 'Most projects in this repo are 32-bit oriented. Win32 is strongly recommended.'
}

$buildList = if ($IncludeLegacySln) {
    $solutionMeta
} else {
    $solutionMeta | Where-Object { $_.Kind -eq 'vcxproj' }
}

Info "Starting build for $($buildList.Count) solutions (Configuration=$Configuration, Platform=$Platform)"
if ($ForcePlatformToolset) {
    Info "Forcing PlatformToolset=$ForcePlatformToolset"
}

$results = @()
foreach ($entry in $buildList) {
    $rel = $entry.Path.Substring($repoRoot.Path.Length + 1)
    Info "Building $rel"

    $propertyArg = "/p:Configuration=$Configuration;Platform=$Platform"
    if ($ForcePlatformToolset) {
        $propertyArg += ";PlatformToolset=$ForcePlatformToolset"
    }

    $msbuildCmd = ('"{0}" -arch=x86 && msbuild "{1}" /m /t:Build {2}' -f $vsDevCmd, $entry.Path, $propertyArg)
    & cmd.exe /c $msbuildCmd
    $code = $LASTEXITCODE

    $results += [pscustomobject]@{
        Solution = $rel
        ExitCode = $code
    }

    if ($code -eq 0) {
        Ok "Built: $rel"
    } else {
        Warn "Failed: $rel (exit code $code)"
    }
}

Write-Host ''
Info 'Build summary:'
$results | ForEach-Object {
    if ($_.ExitCode -eq 0) {
        Write-Host ("  [OK]   {0}" -f $_.Solution) -ForegroundColor Green
    } else {
        Write-Host ("  [FAIL] {0} (code {1})" -f $_.Solution, $_.ExitCode) -ForegroundColor Red
    }
}

$failed = @($results | Where-Object { $_.ExitCode -ne 0 })
if ($failed.Count -gt 0) {
    exit 2
}

exit 0
