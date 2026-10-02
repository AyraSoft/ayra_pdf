# setup_pdfium.ps1 -- install pinned PDFium for ayra_pdf on Windows.
# Source of truth: third_party/pdfium/pdfium_manifest.json

[CmdletBinding()]
param(
    [string]$Platform = '',
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Log { param($Msg) Write-Host "[ayra_pdf] $Msg" -ForegroundColor Cyan }
function Write-Ok  { param($Msg) Write-Host "  [OK] $Msg" -ForegroundColor Green }
function Write-Err { param($Msg) Write-Host "  [ERROR] $Msg" -ForegroundColor Red }

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ModuleDir = Split-Path -Parent $ScriptDir
$PdfiumDir = Join-Path $ModuleDir 'third_party\pdfium'
$ManifestPath = Join-Path $PdfiumDir 'pdfium_manifest.json'
$TempDir = Join-Path $ModuleDir 'third_party\.pdfium_download_tmp'

if (-not (Test-Path $ManifestPath)) { Write-Err "Manifest non trovato: $ManifestPath"; exit 1 }
$Manifest = Get-Content -Raw -Path $ManifestPath | ConvertFrom-Json

if ([string]::IsNullOrEmpty($Platform))
{
    $arch = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture

    if ($arch -eq [System.Runtime.InteropServices.Architecture]::X64)
    {
        $Platform = 'x64'
    }
    elseif ($arch -eq [System.Runtime.InteropServices.Architecture]::X86)
    {
        $Platform = 'x86'
    }
    elseif ($arch -eq [System.Runtime.InteropServices.Architecture]::Arm64)
    {
        $Platform = 'arm64'
    }
    else
    {
        Write-Err "Architettura non supportata: $arch"
        exit 1
    }
}

$AssetKey = "win-$Platform"
$AssetProperty = $Manifest.assets.PSObject.Properties[$AssetKey]
if ($null -eq $AssetProperty)
{
    Write-Err "Architettura Windows non presente nel manifest: $AssetKey"
    exit 1
}
$Asset = $AssetProperty.Value

$Tar = Get-Command tar.exe -ErrorAction SilentlyContinue
if ($null -eq $Tar) { Write-Err 'tar.exe non trovato; richiesto per gli archivi .tgz'; exit 1 }

$ArchiveName = [System.IO.Path]::GetFileName(([Uri][string]$Asset.url).AbsolutePath)
$ArchivePath = Join-Path $TempDir $ArchiveName
$ExtractDir = Join-Path $TempDir "ext_$AssetKey"
$OutDir = Join-Path (Join-Path $PdfiumDir 'win') $Platform
$RuntimeName = [System.IO.Path]::GetFileName([string]$Asset.runtime_path)
$LinkName = [System.IO.Path]::GetFileName([string]$Asset.link_path)
$OutDll = Join-Path $OutDir $RuntimeName
$OutLib = Join-Path $OutDir $LinkName
$StampPath = Join-Path $OutDir '.pdfium-installed'
$IncludeDest = Join-Path $PdfiumDir 'include'
$ExpectedHash = ([string]$Asset.sha256).ToLowerInvariant()
$ExpectedStamp = "$($Manifest.version)|$AssetKey|$ExpectedHash"

Write-Log "PDFium $($Manifest.version) / $AssetKey"

$StampMatches = (Test-Path $StampPath) -and ((Get-Content -Raw $StampPath).Trim() -eq $ExpectedStamp)

if ((Test-Path $OutDll) -and (Test-Path $OutLib) -and $StampMatches -and (-not $Force))
{
    Write-Ok "Gia' installato e coerente col manifest: $OutDir"
    exit 0
}

New-Item -ItemType Directory -Force -Path $TempDir | Out-Null
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

Write-Log "Download: $ArchiveName"
try
{
    $client = New-Object System.Net.WebClient
    $client.DownloadFile([string]$Asset.url, $ArchivePath)
}
catch
{
    Write-Err "Download fallito: $($Asset.url)"
    Write-Err $_.Exception.Message
    exit 1
}

$ActualHash = (Get-FileHash -Path $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($ActualHash -ne $ExpectedHash)
{
    Remove-Item $ArchivePath -Force -ErrorAction SilentlyContinue
    Write-Err "SHA256 non valido per $ArchiveName"
    Write-Err "Atteso: $ExpectedHash"
    Write-Err "Letto:  $ActualHash"
    exit 1
}
Write-Ok 'SHA256 verificato'

if (Test-Path $ExtractDir) { Remove-Item $ExtractDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $ExtractDir | Out-Null
& $Tar.Source -xzf $ArchivePath -C $ExtractDir
if ($LASTEXITCODE -ne 0) { Write-Err "Estrazione fallita: $ArchiveName"; exit 1 }

$SourceDll = Join-Path $ExtractDir ([string]$Asset.runtime_path)
$SourceLib = Join-Path $ExtractDir ([string]$Asset.link_path)
$SourceInclude = Join-Path $ExtractDir 'include'
if (-not (Test-Path $SourceDll)) { Write-Err "DLL non trovata: $SourceDll"; exit 1 }
if (-not (Test-Path $SourceLib)) { Write-Err "Import library non trovata: $SourceLib"; exit 1 }
if (-not (Test-Path (Join-Path $SourceInclude 'fpdfview.h'))) { Write-Err "Header non trovati: $SourceInclude"; exit 1 }

Copy-Item $SourceDll $OutDll -Force
Copy-Item $SourceLib $OutLib -Force
Set-Content -Path $StampPath -Value $ExpectedStamp -NoNewline
New-Item -ItemType Directory -Force -Path $IncludeDest | Out-Null
Copy-Item -Path (Join-Path $SourceInclude '*') -Destination $IncludeDest -Recurse -Force
Remove-Item $ArchivePath -Force

Write-Ok "Runtime: $OutDll"
Write-Ok "Import:  $OutLib"
Write-Ok "Headers: $IncludeDest"
Write-Log 'Linka pdfium.dll.lib e distribuisci pdfium.dll accanto al binario reale del target.'
