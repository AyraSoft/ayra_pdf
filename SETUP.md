# Setup — ayra_pdf

## 1. Dipendenze JUCE

Il core dipende da `juce_graphics`. Per headless definire `AYRA_PDF_HEADLESS=1`.
Per `PdfViewComponent` il target consumer deve includere `juce_gui_basics`.
`juce_gui_extra` non e' richiesto.

## 2. Apple

macOS/iOS usano CoreGraphics + PDFKit di sistema. Nessun PDFium e' richiesto.

## 3. PDFium pinned

La source of truth e' `third_party/pdfium/pdfium_manifest.json`.
Il manifest pinna provider, tag, URL, SHA-256 e path runtime/linker.
Gli header vendorizzati in `third_party/pdfium/include/` devono appartenere alla stessa versione.
Un upgrade PDFium deve aggiornare manifest + header nello stesso cambiamento.

## 4. Windows

```powershell
.\scripts\setup_pdfium.ps1 -Platform x64
```

Supportati: `x64`, `x86`, `arm64`.
Output:

```text
third_party/pdfium/win/<arch>/pdfium.dll
third_party/pdfium/win/<arch>/pdfium.dll.lib
```

Linkare `pdfium.dll.lib` e distribuire `pdfium.dll` accanto al binario reale del target.

## 5. Linux

```bash
./scripts/setup_pdfium.sh --platform linux
./scripts/setup_pdfium.sh --platform linux --arch arm64
```

Output: `third_party/pdfium/linux/<arch>/libpdfium.so`.
La `.so` e' shared: linkarla e renderla disponibile al runtime, tipicamente con RPATH `$ORIGIN`.

## 6. Android

```bash
./scripts/setup_pdfium.sh --platform android --arch arm64
./scripts/setup_pdfium.sh --platform android --arch all
```

Mapping:
- `arm` -> `armeabi-v7a`;
- `arm64` -> `arm64-v8a`;
- `x86` -> `x86`;
- `x64` -> `x86_64`.

Output: `third_party/pdfium/android/<abi>/libpdfium.so`.
La `.so` deve essere linkata e inclusa nel package Android per la stessa ABI.

## 7. Runtime contract

- load/render/search/save non sono realtime-safe;
- un `PdfDocument` condiviso col widget non va mutato concorrentemente dal caller;
- PDFium serializza tutte le API FPDF process-wide;
- Apple serializza le API per istanza.

## 8. Verification

Vedere `_docs/08_verification_v01.md`.
