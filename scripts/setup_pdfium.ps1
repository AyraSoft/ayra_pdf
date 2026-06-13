# ==============================================================================
# setup_pdfium.ps1 -- Scarica e installa PDFium precompilato per il modulo
#                     ayra_pdf su Windows
#
# Utilizzo (PowerShell):
#   .\setup_pdfium.ps1                           # x64, versione default
#   .\setup_pdfium.ps1 -Platform x86             # forza x86
#   .\setup_pdfium.ps1 -Version "chromium/6721"  # versione specifica
#   .\setup_pdfium.ps1 -Force                    # ri-scarica se gia' presente
#
# Output:
#   third_party\pdfium\include\       (headers, committabili in git)
#   third_party\pdfium\win\pdfium.dll (DLL, NON in git)
#   third_party\pdfium\win\pdfium.lib (import lib, NON in git)
#
# Dipendenze: PowerShell 5.1+ (Windows 10 built-in)
# Fonte:      https://github.com/bblanchon/pdfium-binaries
# ==============================================================================

[CmdletBinding()]
param(
    [ValidateSet('x64', 'x86')]
    [string]$Platform = '',

    [string]$Version = '',

    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ------------------------------------------------------------------------------
# Percorsi base
# ------------------------------------------------------------------------------
$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$ModuleDir  = Split-Path -Parent $ScriptDir
$ThirdParty = Join-Path $ModuleDir 'third_party\pdfium'
$TempDir    = Join-Path $ModuleDir 'third_party\.pdfium_download_tmp'
$GithubRepo = 'bblanchon/pdfium-binaries'
$GithubApi  = "https://api.github.com/repos/$GithubRepo/releases/latest"
$GithubBase = "https://github.com/$GithubRepo/releases/download"

# ------------------------------------------------------------------------------
# Funzioni di logging con colori
# ------------------------------------------------------------------------------
function Write-Log   { param($Msg) Write-Host "[ayra_pdf] $Msg" -ForegroundColor Cyan }
function Write-Ok    { param($Msg) Write-Host "  [OK] $Msg"     -ForegroundColor Green }
function Write-Warn  { param($Msg) Write-Host "  [WARN] $Msg"   -ForegroundColor Yellow }
function Write-Err   { param($Msg) Write-Host "  [ERROR] $Msg"  -ForegroundColor Red }
function Write-Hdr   { param($Msg) Write-Host $Msg              -ForegroundColor White }

# ------------------------------------------------------------------------------
# Rileva architettura se non specificata
# ------------------------------------------------------------------------------
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
    else
    {
        Write-Err "Architettura non supportata: $arch"
        Write-Err "Usa -Platform x64 o -Platform x86"
        exit 1
    }
}

# ------------------------------------------------------------------------------
# Risolvi versione
# ------------------------------------------------------------------------------
function Resolve-Version
{
    if (-not [string]::IsNullOrEmpty($Version))
    {
        # Normalizza: "chromium/6721" -> "chromium%2F6721"
        return $Version.Replace('/', '%2F')
    }

    Write-Log "Query GitHub API per versione piu' recente..."
    try
    {
        $response = Invoke-RestMethod -Uri $GithubApi -UseBasicParsing
        $rawTag   = $response.tag_name
        if ([string]::IsNullOrEmpty($rawTag)) { throw "tag vuoto" }
        return $rawTag.Replace('/', '%2F')
    }
    catch
    {
        Write-Warn "GitHub API non raggiungibile -- uso fallback chromium/6721"
        return 'chromium%2F6721'
    }
}

$VersionEncoded = Resolve-Version
$VersionDisplay = $VersionEncoded.Replace('%2F', '/')

# ------------------------------------------------------------------------------
# Costruisci nomi archivio per Windows
# pdfium-win-x64.zip contiene pdfium.dll + pdfium.lib + include/
# ------------------------------------------------------------------------------
$ArchiveName = "pdfium-win-$Platform.zip"
$DownloadUrl = "$GithubBase/$VersionEncoded/$ArchiveName"

$OutDir    = Join-Path $ThirdParty 'win'
$OutDll    = Join-Path $OutDir 'pdfium.dll'
$OutLib    = Join-Path $OutDir 'pdfium.lib'
$IncDest   = Join-Path $ThirdParty 'include'

# ------------------------------------------------------------------------------
# Banner
# ------------------------------------------------------------------------------
Write-Host ""
Write-Hdr "======================================================"
Write-Hdr "  PDFium Setup -- ayra_pdf (Windows)"
Write-Hdr "  Versione:    $VersionDisplay"
Write-Hdr "  Platform:    $Platform"
Write-Hdr "  Module dir:  $ModuleDir"
Write-Hdr "======================================================"
Write-Host ""

# ------------------------------------------------------------------------------
# Skip se gia' presente (e non --Force)
# ------------------------------------------------------------------------------
if ((Test-Path $OutDll) -and (Test-Path $OutLib) -and (-not $Force))
{
    Write-Ok "PDFium gia' installato: $OutDir"
    Write-Ok "Usa -Force per ri-scaricare"
    exit 0
}

# ------------------------------------------------------------------------------
# Crea directory tmp e out
# ------------------------------------------------------------------------------
New-Item -ItemType Directory -Force -Path $TempDir | Out-Null
New-Item -ItemType Directory -Force -Path $OutDir  | Out-Null

# ------------------------------------------------------------------------------
# Download
# ------------------------------------------------------------------------------
$ZipPath = Join-Path $TempDir $ArchiveName

if ((Test-Path $ZipPath) -and (-not $Force))
{
    Write-Ok "Archivio gia' in cache: $ArchiveName"
}
else
{
    Write-Log "Download: $ArchiveName"
    try
    {
        # Usa WebClient per progress in console; Invoke-WebRequest va bene su PS7
        $wc = New-Object System.Net.WebClient
        $wc.DownloadFile($DownloadUrl, $ZipPath)
        Write-Ok "Scaricato: $ArchiveName"
    }
    catch
    {
        Write-Err "Download fallito: $DownloadUrl"
        Write-Err $_.Exception.Message
        exit 1
    }
}

# ------------------------------------------------------------------------------
# Estrai archivio
# ------------------------------------------------------------------------------
$ExtDir = Join-Path $TempDir "ext_win_$Platform"
if (Test-Path $ExtDir) { Remove-Item $ExtDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $ExtDir | Out-Null

Write-Log "Estrazione: $ArchiveName..."
Expand-Archive -Path $ZipPath -DestinationPath $ExtDir -Force
Write-Ok "Estratto in: $ExtDir"

# ------------------------------------------------------------------------------
# Copia DLL e import lib
# Struttura attesa nell'archivio: bin/pdfium.dll, lib/pdfium.lib, include/*.h
# ------------------------------------------------------------------------------
$ExtDll = Get-ChildItem -Path $ExtDir -Recurse -Filter 'pdfium.dll' | Select-Object -First 1
$ExtLib = Get-ChildItem -Path $ExtDir -Recurse -Filter 'pdfium.lib' | Select-Object -First 1

if ($null -eq $ExtDll)
{
    Write-Err "pdfium.dll non trovata nell'archivio estratto"
    exit 1
}
if ($null -eq $ExtLib)
{
    Write-Err "pdfium.lib non trovata nell'archivio estratto"
    exit 1
}

Copy-Item $ExtDll.FullName $OutDll -Force
Copy-Item $ExtLib.FullName $OutLib -Force
Write-Ok "pdfium.dll -> $OutDll"
Write-Ok "pdfium.lib -> $OutLib"

# ------------------------------------------------------------------------------
# Copia headers
# ------------------------------------------------------------------------------
$HeaderRoot = Get-ChildItem -Path $ExtDir -Recurse -Filter 'fpdf_view.h' |
              Select-Object -First 1 |
              ForEach-Object { $_.DirectoryName }

if ([string]::IsNullOrEmpty($HeaderRoot))
{
    Write-Warn "fpdf_view.h non trovato -- cerco qualsiasi .h"
    $HeaderRoot = Get-ChildItem -Path $ExtDir -Recurse -Filter '*.h' |
                  Select-Object -First 1 |
                  ForEach-Object { $_.DirectoryName }
}

if (-not [string]::IsNullOrEmpty($HeaderRoot))
{
    New-Item -ItemType Directory -Force -Path $IncDest | Out-Null
    Copy-Item -Path "$HeaderRoot\*" -Destination $IncDest -Recurse -Force
    $hCount = (Get-ChildItem $IncDest -Recurse -Filter '*.h').Count
    Write-Ok "Header installati ($hCount file): $IncDest"
}
else
{
    Write-Warn "Nessun header trovato nell'archivio"
}

# ------------------------------------------------------------------------------
# Pulizia .zip
# ------------------------------------------------------------------------------
Remove-Item $ZipPath -Force
Write-Ok "Archivio .zip rimosso"

# ------------------------------------------------------------------------------
# Sommario
# ------------------------------------------------------------------------------
Write-Host ""
Write-Hdr "------------------------------------------------------"
Write-Hdr "  Sommario installazione"
Write-Hdr "------------------------------------------------------"

if (Test-Path $OutDll)
{
    $dllSize = (Get-Item $OutDll).Length / 1MB
    Write-Ok ("DLL:     $OutDll ({0:N1} MB)" -f $dllSize)
}
if (Test-Path $OutLib)
{
    $libSize = (Get-Item $OutLib).Length / 1MB
    Write-Ok ("Import:  $OutLib ({0:N1} MB)" -f $libSize)
}
if (Test-Path $IncDest)
{
    $hTotal = (Get-ChildItem $IncDest -Recurse -Filter '*.h').Count
    Write-Ok "Headers: $IncDest ($hTotal file)"
}

Write-Host ""
Write-Hdr "======================================================"
Write-Ok "Setup PDFium completato."
Write-Hdr "======================================================"
Write-Host ""
Write-Log "Prossimi passi:"
Write-Log "  1. Aggiungi al .gitignore:"
Write-Log "       third_party/pdfium/win/"
Write-Log "       third_party/.pdfium_download_tmp/"
Write-Log "  2. Committate third_party/pdfium/include/ (headers stabili)"
Write-Log "  3. Projucer Header Search Paths: `$(MODULE_DIR)\third_party\pdfium\include"
Write-Log "  4. Projucer Extra Library Search Paths: `$(MODULE_DIR)\third_party\pdfium\win"
Write-Log "  5. Projucer Extra Libraries: pdfium"
Write-Log "  6. Copia pdfium.dll accanto all'eseguibile (o nella system PATH)"
Write-Host ""
