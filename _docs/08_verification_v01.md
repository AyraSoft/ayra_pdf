# V01 — Verification Runbook

**Owner dello stato**: `_docs/02_stato_attuale.md`.
Questo file descrive come eseguire V01; non marca autonomamente risultati come passati.

## 1. Vincolo

In questa sessione AI non vengono eseguiti build, test binari o CI.
Ogni voce va eseguita su target reale e riportata nella SSoT.

## 2. Core unit test

`tests/ayra_PdfCoreTests.cpp` viene registrato quando `JUCE_UNIT_TESTS=1`.

```cpp
juce::UnitTestRunner runner;
runner.setAssertOnFailure (false);
runner.setPassesAreLogged (true);
runner.runTestsInCategory ("ayra_pdf", 0x7857);

int failures = 0;

for (int i = 0; i < runner.getNumResults(); ++i)
    if (const auto* result = runner.getResult (i))
        failures += result->failures;

return failures == 0 ? 0 : 1;
```

Copertura fixture generata in memoria:
- MediaBox/CropBox differenti;
- /Rotate 0/90/180/270;
- marker colore per orientamento raster;
- text extraction/search;
- geometry highlight;
- save byte-preserving;
- load fallito transazionale;
- input invalidi.

## 3. macOS

1. Debug + Release;
2. core unit test;
3. load file/memory;
4. zoom/drag/wheel/pinch;
5. Retina e cambio display scale;
6. query durante cambio pagina/zoom;
7. distruzione widget con render/search in-flight;
8. PDFKit text/search da worker;
9. save byte equality.

## 4. iOS

1. build/link CoreGraphics + PDFKit senza AppKit;
2. core smoke/unit test disponibile;
3. load/render/search/save;
4. touch/pinch e device scale;
5. teardown worker in-flight.

## 5. Windows

```powershell
.\scripts\setup_pdfium.ps1 -Platform x64
```

1. Debug + Release;
2. runtime load `pdfium.dll`;
3. core unit test;
4. color marker BGRA;
5. Unicode/search;
6. multi-instance stress PDFium;
7. save equality;
8. malformed/truncated PDF.

## 6. Linux

```bash
./scripts/setup_pdfium.sh --platform linux
```

1. headless build con `AYRA_PDF_HEADLESS=1`;
2. GUI build se applicabile;
3. RPATH/runtime `libpdfium.so`;
4. core unit test;
5. render/search/save;
6. multi-instance stress;
7. malformed PDF.

## 7. Android

```bash
./scripts/setup_pdfium.sh --platform android --arch all
```

Per ogni ABI distribuita:
1. packaging/link/runtime `libpdfium.so`;
2. core smoke;
3. marker colori con reverse byte order;
4. text/search;
5. touch/pinch;
6. teardown worker;
7. malformed PDF.

## 8. Sanitizer

Dove supportato: ASan, UBSan e TSan sul protocollo worker/widget e stress PDFium.

## 9. Esito

V01 passa soltanto con evidenza di build/link, unit test, dependency load, behavior core,
GUI dove presente, concurrency/teardown e malformed input sui target dichiarati.
Riportare data, target, toolchain e risultato in `_docs/02_stato_attuale.md`.
