#!/usr/bin/env bash
# setup_pdfium.sh -- install pinned PDFium for ayra_pdf on Linux/Android.
# Source of truth: third_party/pdfium/pdfium_manifest.json

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
PDFIUM_DIR="${MODULE_DIR}/third_party/pdfium"
MANIFEST="${PDFIUM_DIR}/pdfium_manifest.json"
TEMP_DIR="${MODULE_DIR}/third_party/.pdfium_download_tmp"

PLATFORM=""
ARCH=""
FORCE=0

log()   { printf '[ayra_pdf] %s\n' "$*"; }
ok()    { printf '  [OK] %s\n' "$*"; }
error() { printf '  [ERROR] %s\n' "$*" >&2; }

usage()
{
    cat <<'EOF'
Usage:
  setup_pdfium.sh --platform linux [--arch <manifest-suffix>] [--force]
  setup_pdfium.sh --platform android --arch <manifest-suffix>|all [--force]

Apple targets use CoreGraphics + PDFKit and do not use PDFium.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --platform) PLATFORM="${2:-}"; shift 2 ;;
        --arch)     ARCH="${2:-}"; shift 2 ;;
        --force)    FORCE=1; shift ;;
        --help|-h)  usage; exit 0 ;;
        *) error "Argomento sconosciuto: $1"; usage; exit 1 ;;
    esac
done

for cmd in curl tar python3; do
    command -v "$cmd" >/dev/null 2>&1 || { error "Dipendenza mancante: $cmd"; exit 1; }
done
[[ -f "${MANIFEST}" ]] || { error "Manifest non trovato: ${MANIFEST}"; exit 1; }

if [[ -z "${PLATFORM}" ]]; then
    if [[ "$(uname -s)" == "Linux" ]]; then PLATFORM="linux";
    else
        error 'Specifica --platform linux oppure --platform android.'
        error 'macOS/iOS non richiedono PDFium.'
        exit 1
    fi
fi

case "${PLATFORM}" in linux|android) ;; *) error "Piattaforma non supportata: ${PLATFORM}"; exit 1 ;; esac

detect_linux_arch()
{
    case "$(uname -m)" in
        x86_64) echo x64 ;;
        i386|i686) echo x86 ;;
        aarch64|arm64) echo arm64 ;;
        armv7*|armv8l) echo arm ;;
        *) return 1 ;;
    esac
}

if [[ "${PLATFORM}" == linux && -z "${ARCH}" ]]; then
    ARCH="$(detect_linux_arch)" || { error "Architettura Linux non supportata: $(uname -m)"; exit 1; }
fi
if [[ "${PLATFORM}" == android && -z "${ARCH}" ]]; then
    error 'Per Android specifica --arch <manifest-suffix>|all.'; exit 1
fi
if [[ "${PLATFORM}" == linux && "${ARCH}" == all ]]; then
    error '--arch all e ammesso solo per Android'; exit 1
fi

manifest_value()
{
    python3 - "$MANIFEST" "$1" "$2" <<'PY'
import json, sys
with open(sys.argv[1], 'r', encoding='utf-8') as handle:
    manifest = json.load(handle)
asset = manifest['assets'].get(sys.argv[2])
if asset is None: raise SystemExit(2)
value = asset.get(sys.argv[3])
if value is None: raise SystemExit(3)
print(value)
PY
}

sha256_file()
{
    if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{ print $1 }';
    elif command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | awk '{ print $1 }';
    else error 'Serve sha256sum oppure shasum'; return 1; fi
}

install_headers()
{
    local source_include="$1/include"
    [[ -f "$source_include/fpdfview.h" ]] || { error "Header non trovati: $source_include"; return 1; }
    mkdir -p "$PDFIUM_DIR/include"
    cp -R "$source_include/." "$PDFIUM_DIR/include/"
}

install_one()
{
    local platform="$1" arch="$2" key url expected_hash runtime_path abi
    local archive_name archive_path extract_dir out_dir out_lib stamp_path expected_stamp actual_hash
    key="${platform}-${arch}"
    url="$(manifest_value "$key" url)" || { error "Asset non presente: $key"; return 1; }
    expected_hash="$(manifest_value "$key" sha256)"
    runtime_path="$(manifest_value "$key" runtime_path)"
    abi=""
    archive_name="$(basename "$url")"
    archive_path="$TEMP_DIR/$archive_name"
    extract_dir="$TEMP_DIR/ext_$key"

    if [[ "$platform" == linux ]]; then
        out_dir="$PDFIUM_DIR/linux/$arch"
    else
        abi="$(manifest_value "$key" abi)" || {
            error "ABI Android mancante nel manifest: $key"
            return 1
        }
        out_dir="$PDFIUM_DIR/android/$abi"
    fi
    out_lib="$out_dir/libpdfium.so"
    stamp_path="$out_dir/.pdfium-installed"
    expected_stamp="$VERSION|$key|$expected_hash"

    if [[ -f "$out_lib" && -f "$stamp_path" && "$FORCE" -eq 0 ]]; then
        if [[ "$(cat "$stamp_path")" == "$expected_stamp" ]]; then
            ok "Gia installato e coerente col manifest: $out_lib"
            return 0
        fi
    fi
    mkdir -p "$TEMP_DIR" "$out_dir"
    log "Download $key: $archive_name"
    curl -fL --progress-bar "$url" -o "$archive_path"
    actual_hash="$(sha256_file "$archive_path")"
    if [[ "$actual_hash" != "$expected_hash" ]]; then
        rm -f "$archive_path"
        error "SHA256 non valido per $archive_name"
        error "Atteso: $expected_hash"
        error "Letto:  $actual_hash"
        return 1
    fi
    ok "SHA256 verificato: $key"
    rm -rf "$extract_dir"; mkdir -p "$extract_dir"
    tar -xzf "$archive_path" -C "$extract_dir"
    [[ -f "$extract_dir/$runtime_path" ]] || { error "libpdfium.so non trovata: $extract_dir/$runtime_path"; return 1; }
    cp "$extract_dir/$runtime_path" "$out_lib"
    printf '%s' "$expected_stamp" > "$stamp_path"
    install_headers "$extract_dir"
    rm -f "$archive_path"
    ok "Runtime: $out_lib"
}

VERSION="$(python3 - "$MANIFEST" <<'PY'
import json, sys
with open(sys.argv[1], 'r', encoding='utf-8') as handle:
    print(json.load(handle)['version'])
PY
)"
log "PDFium pinned: $VERSION"

if [[ "$PLATFORM" == android && "$ARCH" == all ]]; then
    while IFS= read -r target_arch; do
        [[ -n "$target_arch" ]] || continue
        install_one android "$target_arch"
    done < <(
        python3 - "$MANIFEST" <<'PY'
import json, sys
with open(sys.argv[1], 'r', encoding='utf-8') as handle:
    assets = json.load(handle)['assets']
for key in sorted(assets):
    if key.startswith('android-'):
        print(key[len('android-'):])
PY
    )
else
    install_one "$PLATFORM" "$ARCH"
fi

log 'Setup completato. I binari sono shared e devono essere inclusi nel deployment.'
