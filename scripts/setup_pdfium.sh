#!/usr/bin/env bash
# ==============================================================================
# setup_pdfium.sh -- Scarica e installa PDFium precompilato per il modulo ayra_pdf
#
# Utilizzo:
#   ./setup_pdfium.sh                              # piattaforma corrente, versione default
#   ./setup_pdfium.sh --platform mac               # solo macOS (fat binary arm64+x64)
#   ./setup_pdfium.sh --platform linux             # solo Linux (arch corrente)
#   ./setup_pdfium.sh --platform ios               # solo iOS device (arm64)
#   ./setup_pdfium.sh --version chromium%2F6721    # versione specifica (URL-encoded)
#   ./setup_pdfium.sh --version "chromium/6721"    # oppure con slash (auto-encoded)
#   ./setup_pdfium.sh --force                      # ri-scarica anche se gia' presente
#   ./setup_pdfium.sh --help
#
# Output:
#   third_party/pdfium/include/          (headers, committabili in git)
#   third_party/pdfium/mac/libpdfium.a   (fat binary arm64+x64, NON in git)
#   third_party/pdfium/linux/libpdfium.a (NON in git)
#   third_party/pdfium/ios/libpdfium.a   (NON in git)
#
# Dipendenze: curl, tar, python3 (per JSON), lipo (solo macOS per fat binary)
# Fonte:      https://github.com/bblanchon/pdfium-binaries
# ==============================================================================
set -uo pipefail

# ------------------------------------------------------------------------------
# Percorsi base: MODULE_DIR e' la cartella del modulo (padre di scripts/)
# ------------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
THIRD_PARTY_DIR="${MODULE_DIR}/third_party/pdfium"
TEMP_DIR="${MODULE_DIR}/third_party/.pdfium_download_tmp"
GITHUB_REPO="bblanchon/pdfium-binaries"
GITHUB_API="https://api.github.com/repos/${GITHUB_REPO}/releases/latest"
GITHUB_RELEASES="https://github.com/${GITHUB_REPO}/releases/download"

# ------------------------------------------------------------------------------
# Colori ANSI
# ------------------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

log()   { echo -e "${BLUE}[ayra_pdf]${NC} $*"; }
ok()    { echo -e "${GREEN}  [OK]${NC} $*"; }
warn()  { echo -e "${YELLOW}  [WARN]${NC} $*"; }
error() { echo -e "${RED}  [ERROR]${NC} $*" >&2; }
hdr()   { echo -e "${BOLD}$*${NC}"; }

# ------------------------------------------------------------------------------
# Defaults
# ------------------------------------------------------------------------------
FORCE=0
PLATFORM=""
VERSION_TAG=""

# ------------------------------------------------------------------------------
# Parse argomenti
# ------------------------------------------------------------------------------
print_help()
{
    echo ""
    hdr "  setup_pdfium.sh -- Download PDFium per ayra_pdf"
    echo ""
    echo "  Utilizzo:"
    echo "    $0 [--platform mac|linux|ios] [--version TAG] [--force] [--help]"
    echo ""
    echo "  Opzioni:"
    echo "    --platform  mac|linux|ios  Forza piattaforma (default: auto-detect)"
    echo "    --version   TAG            Versione specifica, es: chromium%2F6721"
    echo "                               (il tag GitHub usa / ma nell'URL va %2F)"
    echo "    --force                    Ri-scarica anche se i file esistono gia'"
    echo "    --help                     Mostra questo aiuto"
    echo ""
    echo "  Esempi:"
    echo "    $0"
    echo "    $0 --platform mac --force"
    echo "    $0 --version chromium%2F6906"
    echo ""
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --force)    FORCE=1;           shift ;;
        --platform) PLATFORM="$2";     shift 2 ;;
        --version)  VERSION_TAG="$2";  shift 2 ;;
        --help|-h)  print_help; exit 0 ;;
        *)
            error "Argomento sconosciuto: $1"
            print_help
            exit 1
            ;;
    esac
done

# ------------------------------------------------------------------------------
# Auto-detect piattaforma se non specificata
# ------------------------------------------------------------------------------
detect_platform()
{
    local os
    os="$(uname -s)"
    case "${os}" in
        Darwin) echo "mac" ;;
        Linux)  echo "linux" ;;
        *)
            error "Piattaforma non supportata: ${os}"
            error "Usa --platform per forzare: mac, linux, ios"
            exit 1
            ;;
    esac
}

[[ -z "${PLATFORM}" ]] && PLATFORM="$(detect_platform)"

# ------------------------------------------------------------------------------
# Verifica dipendenze
# ------------------------------------------------------------------------------
check_deps()
{
    local missing=0
    for cmd in curl tar python3; do
        if ! command -v "${cmd}" &>/dev/null; then
            error "Dipendenza mancante: ${cmd}"
            missing=1
        fi
    done
    if [[ "${PLATFORM}" == "mac" ]]; then
        if ! command -v lipo &>/dev/null; then
            error "lipo non trovato -- necessario per fat binary macOS (installa Xcode CLI tools)"
            missing=1
        fi
    fi
    [[ ${missing} -eq 1 ]] && exit 1
}

check_deps

# ------------------------------------------------------------------------------
# Query versione piu' recente da GitHub API
# ------------------------------------------------------------------------------
resolve_version()
{
    if [[ -n "${VERSION_TAG}" ]]; then
        # Normalizza a URL-encoded: "chromium/6721" -> "chromium%2F6721"
        echo "${VERSION_TAG//\//%2F}"
        return
    fi

    echo -e "${BLUE}[ayra_pdf]${NC} Query GitHub API per versione piu' recente..." >&2
    local raw_tag
    raw_tag="$(curl -sf "${GITHUB_API}" \
        | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['tag_name'])" 2>/dev/null)" || true

    if [[ -z "${raw_tag}" ]]; then
        echo -e "${YELLOW}  [WARN]${NC} GitHub API non raggiungibile -- uso versione fallback chromium/6721" >&2
        raw_tag="chromium/6721"
    fi

    # URL-encode la / nel tag (chromium/NNNN -> chromium%2FNNNN)
    echo "${raw_tag//\//%2F}"
}

VERSION_ENCODED="$(resolve_version)"
# Versione human-readable per i log (decodifica %2F -> /)
VERSION_DISPLAY="${VERSION_ENCODED//%2F//}"
log "Versione PDFium: ${BOLD}${VERSION_DISPLAY}${NC}"

# ------------------------------------------------------------------------------
# Helper: download con progress bar
# $1 = URL, $2 = path destinazione
# ------------------------------------------------------------------------------
download_file()
{
    local url="$1"
    local dest="$2"
    local name
    name="$(basename "${dest}")"

    if [[ -f "${dest}" && ${FORCE} -eq 0 ]]; then
        ok "Gia' presente (usa --force per ri-scaricare): ${name}"
        return 0
    fi

    log "Download: ${name}"
    if ! curl -fL --progress-bar "${url}" -o "${dest}"; then
        error "Download fallito: ${url}"
        return 1
    fi
    ok "Scaricato: ${name}"
}

# ------------------------------------------------------------------------------
# Helper: verifica che il file sia un archivio tar valido
# $1 = path archivio .tgz
# ------------------------------------------------------------------------------
verify_archive()
{
    local archive="$1"
    if ! tar -tzf "${archive}" &>/dev/null; then
        error "Archivio corrotto o non valido: ${archive}"
        return 1
    fi
    ok "Archivio valido: $(basename "${archive}")"
}

# ------------------------------------------------------------------------------
# Helper: estrai la libreria PDFium da un archivio
# $1 = archivio .tgz
# $2 = directory di estrazione temporanea
# Stampa su stdout il path completo della libreria estratta (dylib o a)
# Nota: le release recenti di bblanchon/pdfium-binaries (>= 7xxx) distribuiscono
#       libpdfium.dylib su macOS invece della statica .a. Lo script accetta entrambi.
# ------------------------------------------------------------------------------
extract_archive()
{
    local archive="$1"
    local extract_dir="$2"

    mkdir -p "${extract_dir}"
    tar -xzf "${archive}" -C "${extract_dir}"

    # Cerca prima .dylib, poi .a (preferenza per la dinamica nelle release recenti)
    local lib_path
    lib_path="$(find "${extract_dir}" -name "libpdfium.dylib" | head -1)"
    if [[ -z "${lib_path}" ]]; then
        lib_path="$(find "${extract_dir}" -name "libpdfium.a" | head -1)"
    fi
    if [[ -z "${lib_path}" ]]; then
        error "Libreria PDFium non trovata in: ${archive}"
        error "Atteso: lib/libpdfium.dylib oppure lib/libpdfium.a"
        return 1
    fi

    echo "${lib_path}"
}

# ------------------------------------------------------------------------------
# Helper: copia gli header da una directory estratta a THIRD_PARTY_DIR/include/
# $1 = directory estratta
# $2 = destinazione include/
# ------------------------------------------------------------------------------
install_headers()
{
    local extract_dir="$1"
    local include_dest="$2"

    # Struttura archivi PDFium: include/fpdfview.h (senza underscore, nome canonico)
    # Alcune release usano fpdf_view.h — prova entrambi
    local include_src
    include_src="$(find "${extract_dir}" -maxdepth 3 \( -name "fpdfview.h" -o -name "fpdf_view.h" \) \
        -exec dirname {} \; | head -1)"

    if [[ -z "${include_src}" ]]; then
        warn "Header fpdfview.h non trovato -- fallback su qualsiasi .h"
        include_src="$(find "${extract_dir}" -maxdepth 3 -name "*.h" -exec dirname {} \; | sort -u | head -1)"
    fi

    if [[ -n "${include_src}" ]]; then
        mkdir -p "${include_dest}"
        # Copia ricorsiva: mantiene sottocartelle (es. include/cpp/)
        cp -r "${include_src}/." "${include_dest}/"
        local h_count
        h_count="$(find "${include_dest}" -name "*.h" | wc -l | tr -d ' ')"
        ok "Header installati (${h_count} file): ${include_dest}"
    else
        warn "Nessun header trovato nell'archivio"
    fi
}

# ==============================================================================
# PIATTAFORMA: macOS
# Scarica arm64 + x64, crea fat binary con lipo
# ==============================================================================
install_mac()
{
    log "Piattaforma: ${BOLD}macOS${NC} (fat binary arm64 + x64)"

    local out_dir="${THIRD_PARTY_DIR}/mac"

    # Output: usa stessa estensione della libreria trovata (.dylib o .a)
    # Determinata dopo l'estrazione del primo archivio
    local out_lib=""

    # Controlla se il fat binary esiste già (con entrambe le estensioni)
    if [[ ${FORCE} -eq 0 ]]; then
        if [[ -f "${out_dir}/libpdfium.dylib" ]]; then
            ok "Fat binary gia' presente: ${out_dir}/libpdfium.dylib (usa --force per ri-creare)"
            return 0
        elif [[ -f "${out_dir}/libpdfium.a" ]]; then
            ok "Fat binary gia' presente: ${out_dir}/libpdfium.a (usa --force per ri-creare)"
            return 0
        fi
    fi

    mkdir -p "${TEMP_DIR}" "${out_dir}"

    local url_arm64="${GITHUB_RELEASES}/${VERSION_ENCODED}/pdfium-mac-arm64.tgz"
    local url_x64="${GITHUB_RELEASES}/${VERSION_ENCODED}/pdfium-mac-x64.tgz"
    local tgz_arm64="${TEMP_DIR}/pdfium-mac-arm64.tgz"
    local tgz_x64="${TEMP_DIR}/pdfium-mac-x64.tgz"

    download_file "${url_arm64}" "${tgz_arm64}" || return 1
    download_file "${url_x64}"  "${tgz_x64}"   || return 1

    verify_archive "${tgz_arm64}" || return 1
    verify_archive "${tgz_x64}"   || return 1

    local ext_arm64="${TEMP_DIR}/ext_arm64"
    local ext_x64="${TEMP_DIR}/ext_x64"
    rm -rf "${ext_arm64}" "${ext_x64}"

    log "Estrazione archivi..."
    local lib_arm64 lib_x64
    lib_arm64="$(extract_archive "${tgz_arm64}" "${ext_arm64}")" || return 1
    lib_x64="$(extract_archive "${tgz_x64}" "${ext_x64}")"       || return 1

    ok "libpdfium arm64: ${lib_arm64}"
    ok "libpdfium x64:   ${lib_x64}"

    # Installa headers dall'archivio arm64 (identici in entrambi gli archivi)
    install_headers "${ext_arm64}" "${THIRD_PARTY_DIR}/include"

    # Determina il nome output dalla libreria trovata (.dylib o .a)
    local lib_ext="${lib_arm64##*.}"
    out_lib="${out_dir}/libpdfium.${lib_ext}"

    # Fat binary con lipo (funziona sia con .dylib che con .a)
    log "Creazione fat binary (lipo) -> libpdfium.${lib_ext}..."
    lipo -create "${lib_arm64}" "${lib_x64}" -output "${out_lib}"
    ok "Fat binary creato: ${out_lib}"

    if [[ "${lib_ext}" == "dylib" ]]; then
        # ----------------------------------------------------------------------
        # FIX install_name: i build PDFium ufficiali (bblanchon/pdfium-binaries
        # >= chromium/7000) distribuiscono il .dylib con install_name relativo
        # "./libpdfium.dylib", che fa fallire dyld al runtime perche' la
        # libreria viene cercata nel CWD del processo.
        #
        # Riscriviamo l'install_name a @rpath/libpdfium.dylib cosi' che il
        # consumer possa risolverlo tramite LC_RPATH (di norma @loader_path,
        # ovvero la cartella del binario / del bundle MacOS/).
        # ----------------------------------------------------------------------
        log "Fix install_name -> @rpath/libpdfium.dylib (richiesto per dyld)"
        install_name_tool -id @rpath/libpdfium.dylib "${out_lib}"
        # Ri-firma ad-hoc: install_name_tool invalida la firma esistente
        codesign --force --sign - "${out_lib}" 2>/dev/null || true
        local id_check
        id_check="$(otool -D "${out_lib}" | tail -1)"
        ok "install_name: ${id_check}"

        warn "Nota: release PDFium ${VERSION_DISPLAY} distribuisce libreria dinamica (.dylib)."
        warn "      Il .dylib deve essere bundled nel VST3/AU/Standalone al deploy."
        warn "      Vedi SETUP.md sezione 'macOS / iOS - PDFium dylib deployment'."
    fi

    # Pulizia .tgz; mantieni dir estratte per cache (re-run senza --force non ri-scarica)
    rm -f "${tgz_arm64}" "${tgz_x64}"
    ok "Archivi .tgz rimossi (dir estratte in cache per riuso)"
}

# ==============================================================================
# PIATTAFORMA: iOS (device arm64)
# ==============================================================================
install_ios()
{
    log "Piattaforma: ${BOLD}iOS${NC} (arm64 device)"

    local out_dir="${THIRD_PARTY_DIR}/ios"
    local out_lib="${out_dir}/libpdfium.a"

    if [[ -f "${out_lib}" && ${FORCE} -eq 0 ]]; then
        ok "Libreria iOS gia' presente: ${out_lib}"
        return 0
    fi

    mkdir -p "${TEMP_DIR}" "${out_dir}"

    local url="${GITHUB_RELEASES}/${VERSION_ENCODED}/pdfium-ios-arm64.tgz"
    local tgz="${TEMP_DIR}/pdfium-ios-arm64.tgz"

    download_file "${url}" "${tgz}" || return 1
    verify_archive "${tgz}" || return 1

    local ext_dir="${TEMP_DIR}/ext_ios"
    rm -rf "${ext_dir}"

    local lib_path
    lib_path="$(extract_archive "${tgz}" "${ext_dir}")" || return 1

    install_headers "${ext_dir}" "${THIRD_PARTY_DIR}/include"
    cp "${lib_path}" "${out_lib}"
    ok "Libreria iOS installata: ${out_lib}"

    rm -f "${tgz}"
    ok "Archivio .tgz rimosso"
}

# ==============================================================================
# PIATTAFORMA: Linux
# Rileva arch corrente: x86_64 -> x64, aarch64/arm64 -> arm64
# ==============================================================================
install_linux()
{
    local machine
    machine="$(uname -m)"
    local pdfium_arch
    case "${machine}" in
        x86_64)        pdfium_arch="x64" ;;
        aarch64|arm64) pdfium_arch="arm64" ;;
        *)
            error "Architettura Linux non supportata: ${machine}"
            error "Supportati: x86_64, aarch64"
            exit 1
            ;;
    esac

    log "Piattaforma: ${BOLD}Linux${NC} (${machine} -> ${pdfium_arch})"

    local out_dir="${THIRD_PARTY_DIR}/linux"
    local out_lib="${out_dir}/libpdfium.a"

    if [[ -f "${out_lib}" && ${FORCE} -eq 0 ]]; then
        ok "Libreria Linux gia' presente: ${out_lib}"
        return 0
    fi

    mkdir -p "${TEMP_DIR}" "${out_dir}"

    local url="${GITHUB_RELEASES}/${VERSION_ENCODED}/pdfium-linux-${pdfium_arch}.tgz"
    local tgz="${TEMP_DIR}/pdfium-linux-${pdfium_arch}.tgz"

    download_file "${url}" "${tgz}" || return 1
    verify_archive "${tgz}" || return 1

    local ext_dir="${TEMP_DIR}/ext_linux"
    rm -rf "${ext_dir}"

    local lib_path
    lib_path="$(extract_archive "${tgz}" "${ext_dir}")" || return 1

    install_headers "${ext_dir}" "${THIRD_PARTY_DIR}/include"
    cp "${lib_path}" "${out_lib}"
    ok "Libreria Linux installata: ${out_lib}"

    rm -f "${tgz}"
    ok "Archivio .tgz rimosso"
}

# ==============================================================================
# Funzione di sommario (chiamata dopo install_*)
# ==============================================================================
print_summary()
{
    echo ""
    hdr "------------------------------------------------------"
    hdr "  Sommario installazione"
    hdr "------------------------------------------------------"

    # Headers
    if [[ -d "${THIRD_PARTY_DIR}/include" ]]; then
        local h_count
        h_count="$(find "${THIRD_PARTY_DIR}/include" -name "*.h" | wc -l | tr -d ' ')"
        ok "Headers (${h_count} file): ${THIRD_PARTY_DIR}/include/"
    else
        warn "Directory include/ non trovata"
    fi

    # Librerie per piattaforma (salta include/ nel loop)
    for plat_dir in "${THIRD_PARTY_DIR}"/*/; do
        local plat_name
        plat_name="$(basename "${plat_dir}")"
        [[ "${plat_name}" == "include" ]] && continue
        # Cerca .a o .dylib o .so
        local candidate_lib=""
        for ext in dylib a so; do
            if [[ -f "${plat_dir}libpdfium.${ext}" ]]; then
                candidate_lib="${plat_dir}libpdfium.${ext}"
                break
            fi
        done
        if [[ -n "${candidate_lib}" ]]; then
            local lib_size
            lib_size="$(du -sh "${candidate_lib}" | cut -f1)"
            ok "Libreria [${plat_name}]: ${candidate_lib} (${lib_size})"
        fi
    done

    # lipo info per mac fat binary (sia .a che .dylib)
    local mac_lib=""
    for ext in dylib a; do
        if [[ -f "${THIRD_PARTY_DIR}/mac/libpdfium.${ext}" ]]; then
            mac_lib="${THIRD_PARTY_DIR}/mac/libpdfium.${ext}"
            break
        fi
    done
    if [[ -n "${mac_lib}" ]]; then
        echo ""
        log "Architetture fat binary (${mac_lib##*/}):"
        lipo -info "${mac_lib}"
    fi

    # Prime righe dell'header principale
    local main_header="${THIRD_PARTY_DIR}/include/fpdfview.h"
    [[ ! -f "${main_header}" ]] && main_header="${THIRD_PARTY_DIR}/include/fpdf_view.h"
    if [[ -f "${main_header}" ]]; then
        echo ""
        log "Header principale ($(basename "${main_header}")) -- prime 5 righe:"
        head -5 "${main_header}"
    fi

    echo ""
    hdr "======================================================"
    ok "Setup PDFium completato."
    hdr "======================================================"
    echo ""
    log "Prossimi passi:"
    log "  1. Aggiungi al .gitignore:"
    log "       third_party/pdfium/mac/"
    log "       third_party/pdfium/linux/"
    log "       third_party/pdfium/ios/"
    log "       third_party/.pdfium_download_tmp/"
    log "  2. Committate third_party/pdfium/include/ (headers stabili)"
    log "  3. Projucer / CMake Header Search Paths: \$(MODULE_DIR)/third_party/pdfium/include"
    log "  4. Projucer / CMake Extra Library Search Paths: \$(MODULE_DIR)/third_party/pdfium/mac"
    log "  5. Projucer / CMake Extra Libraries: pdfium"
    echo ""
}

# ==============================================================================
# MAIN
# ==============================================================================
echo ""
hdr "======================================================"
hdr "  PDFium Setup -- ayra_pdf"
hdr "  Versione:    ${VERSION_DISPLAY}"
hdr "  Piattaforma: ${PLATFORM}"
hdr "  Module dir:  ${MODULE_DIR}"
hdr "======================================================"
echo ""

case "${PLATFORM}" in
    mac)   install_mac   ;;
    ios)   install_ios   ;;
    linux) install_linux ;;
    *)
        error "Piattaforma non riconosciuta: ${PLATFORM}"
        error "Valori validi: mac, linux, ios"
        exit 1
        ;;
esac

print_summary
