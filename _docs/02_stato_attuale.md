# Stato Attuale — ayra_pdf

**Autore**: Ayra Soft  
**Data aggiornamento**: 2026-05-26

---

## Tabella riepilogativa

| File / Classe | Stato | Piattaforma | Fase |
|---|---|---|---|
| `engine/ayra_PdfPage.h` | ✅ completo | tutte | 1 |
| `engine/ayra_PdfSearchResult.h` | ✅ completo | tutte | 1 |
| `engine/ayra_PdfDocument.h` | ✅ interfaccia completa | tutte | 1 |
| `engine/ayra_PdfDocument.cpp` | ❌ stub (tutti jassertfalse) | tutte | 4 |
| `renderer/ayra_PdfRenderer.h` | ✅ interfaccia completa | tutte | 1 |
| `renderer/mac/ayra_MacPdfRenderer.h` | ✅ dichiarazione completa | macOS/iOS | 1 |
| `renderer/mac/ayra_MacPdfRenderer.mm` | ❌ stub (jassertfalse), solo close() parziale | macOS/iOS | 2 |
| `renderer/pdfium/ayra_PdfiumRenderer.h` | ✅ dichiarazione completa | Win/Linux/Android | 1 |
| `renderer/pdfium/ayra_PdfiumRenderer.cpp` | ❌ stub (jassertfalse), solo close() parziale | Win/Linux/Android | 3 |
| `widgets/ayra_PdfViewComponent.h` | ✅ interfaccia completa | tutte | 1 |
| `widgets/ayra_PdfViewComponent.cpp` | ⚠️ parziale (setDocument funziona, resto stub) | tutte | 5 |
| `widgets/look_and_feel/ayra_PdfLookAndFeelMethods.h` | ✅ completo | tutte | 1 |
| `widgets/look_and_feel/ayra_PdfDefaultLookAndFeel.h/.cpp` | ✅ completo | tutte | 1 |
| `pdf_component/PDFComponent.h/.cpp` | ✅ funzionante (LEGACY) | macOS | - |
| `pdf_component/Mac_PDF_core/PDFView.m` | ✅ funzionante (LEGACY) | macOS | - |
| `pdf_component/Mac_PDF_core/MacPDFComponent.mm` | 🟠 funzionante con bug | macOS | - |
| `third_party/pdfium/include/` | ✅ header presenti (git) | Win/Linux | - |
| `third_party/pdfium/mac/libpdfium.dylib` | ✅ installata (chromium/7857) | macOS | - |
| `scripts/setup_pdfium.sh` | ✅ funzionante | macOS/Linux | - |
| `scripts/setup_pdfium.ps1` | ✅ funzionante | Windows | - |

---

## Cosa funziona oggi

**Solo il backend legacy macOS** tramite `PDFComponent` / `MacPDFViewComponent` / `PDFView`.

Funzionalita' operative:
- Apertura file PDF da path stringa (`loadDocument`)
- Apertura da buffer in memoria (`loadDocumentFromMemoryBlock`)
- Navigazione pagine (`setPageNumber`, `getTotPagesNum`, `getCurrentPageOnScreen`)
- Zoom con punto di handle (`setCurrentPageZoom`) — con il bug dell'anchor point
- Pan (`setCurrentPageTopLeftPosition`, `getCurrentPageTopLeftPosition`)
- Bounding box pagina corrente (`getCurrentPageBounds`)
- Export su file (`exportCurrentDocument`) — funzionante, usa `CGPDFContextCreateWithURL` con URL reale
- Dimensioni documento (`getDocumentWidth`, `getDocumentHeight`)

Funzionalita' non operative (mai implementate nel v1):
- `getMemoryBlockFromDocument` — rotto (vedi Bug 2)
- Ricerca testuale — mai implementata
- Estrazione testo — mai implementata
- Inspector metadati pagina (rotazione, tipo media box) — non esposti

---

## Bug noti — ordinati per priorita'

### Bug 1 — Shell injection in LinuxPDFViewComponent

**Severita'**: critica (vulnerabilita' di sicurezza)  
**File**: `pdf_component/Linux_PDF_core/LinuxPDFViewComponent.h` (stub)  
**Riga**: nella funzione `loadDocument` del componente Linux legacy  
**Descrizione**: il path del file veniva interpolato direttamente in una stringa passata a `system()`:

```cpp
// VULNERABILE
std::string cmd = "pdftocairo -png \"" + filePath + "\" /tmp/ayra_pdf_out";
system(cmd.c_str());
```

Se `filePath` contiene `;`, `&&`, `|`, `$()` o backtick, il codice aggiuntivo viene eseguito
con i permessi del processo host (DAW). Su Linux questo e' un rischio concreto se il plugin
e' usato con file PDF da fonti esterne.

**Soluzione**: il bug e' moot con il refactoring — `PdfiumRenderer` su Linux non usa subprocess.
Il file legacy non e' incluso nel build corrente.

---

### Bug 2 — getMemoryBlockFromDocument rotto

**Severita'**: alta (funzione restituisce dati corrotti silenziosamente)  
**File**: `pdf_component/Mac_PDF_core/MacPDFComponent.mm`  
**Riga**: 255-282  
**Descrizione**: la funzione usa `CGPDFContextCreateWithURL` con `@"dummyURL"`:

```objc
// v1 MacPDFComponent.mm:260 — rotto
CGContextRef pdfContext = CGPDFContextCreateWithURL(
    (__bridge CFURLRef)[NSURL URLWithString:@"dummyURL"], NULL, NULL);
```

`@"dummyURL"` non e' un file URL valido: `[NSURL URLWithString:@"dummyURL"]` ritorna un NSURL
con scheme `nil`, che non puo' essere usato come destinazione di scrittura. Il contesto viene
creato (non nil) ma i dati scritti nel "file" vengono silenziosamente ignorati da CoreGraphics.

Alla fine della funzione:
```objc
// v1 MacPDFComponent.mm:281 — produce junk
destData = juce::MemoryBlock { (void*)pdfData, sizeof(CGPDFDocumentRef) };
```

Questa riga copia i primi `sizeof(CGPDFDocumentRef)` byte (8 byte su 64-bit) del buffer
`pdfData` (che e' vuoto), producendo un `MemoryBlock` con 8 byte casuali dallo stack.

**Causa**: probabilmente copia di un pattern da un tutorial errato. Il pattern corretto
richiede `CGDataConsumerCreate` con un callback di scrittura verso `NSMutableData`.

**Soluzione**: vedi `03_fase2_MacPdfRenderer.md` sezione "Fix saveToMemory()".

---

### Bug 3 — Zoom anchor point formula errata

**Severita'**: media (UX degradata ma non crash)  
**File**: `pdf_component/Mac_PDF_core/MacPDFComponent.mm`  
**Riga**: 152-166 (`setCurrentPageZoom`)  
**Descrizione**:

```cpp
// v1 — formula errata
float xOffset = handlePoint.getX() - (handlePoint.getX() * pdfView.zoomLevel);
float yOffset = handlePoint.getY() - (handlePoint.getY() * pdfView.zoomLevel);
pdfView.topLeftOrigin = CGPointMake(origin.getX() + xOffset, origin.getY() + yOffset);
```

Il problema e' che `handlePoint` qui e' il mouse in viewport-space, ma `origin` e' il
`topLeftOrigin` del PDF anche (anch'esso in viewport-space). La formula calcola un offset
come `handle - handle * newZoom` che ignora completamente il vecchio zoom. L'ancora e' sempre
calcolata come se la pagina fosse posizionata all'origine (0,0), non alla posizione corrente.

**Effetto visibile**: durante lo zoom con pinch o scroll, la pagina scivola verso l'angolo
in alto a sinistra invece di rimanere ancorata al punto di contatto del mouse.

**Soluzione**: convertire l'anchor da viewport-space a PDF-space prima di applicare il nuovo zoom.
Vedi `03_fase2_MacPdfRenderer.md` sezione "Fix zoom anchor point".

---

### Bug 4 — Conteggio pagine approssimato su Windows/Linux legacy

**Severita'**: bassa (impatta solo piattaforme con stub)  
**File**: `pdf_component/Windows_PDF_core/` e `pdf_component/Linux_PDF_core/` (stub)  
**Descrizione**: il conteggio delle pagine avveniva cercando la stringa `/Page` nel buffer
grezzo del PDF senza parsing strutturale. I documenti con form fields, risorse XObject,
o strutture ad albero complesse producevano un conteggio errato (tipicamente superiore al reale).

**Soluzione**: il bug e' moot con il refactoring — `FPDF_GetPageCount()` conta le pagine
strutturalmente dal cross-reference table del PDF.

---

### Bug 5 — getCurrentPageBounds — origine CoreGraphics non convertita

**Severita'**: bassa (impatta documenti con MediaBox non in (0,0))  
**File**: `pdf_component/Mac_PDF_core/MacPDFComponent.mm`  
**Riga**: 177-183  
**Descrizione**: `CGPDFPageGetBoxRect` ritorna il rettangolo in PDF user space con origine
in basso a sinistra (Y-up). `juce::Rectangle` usa Y-down. La funzione copia i valori senza
conversione:

```cpp
// v1 — origine non convertita
return juce::Rectangle<float> {
    (float)mediaBox.origin.x,
    (float)mediaBox.origin.y,   // Y CoreGraphics != Y juce
    (float)mediaBox.size.width,
    (float)mediaBox.size.height
};
```

Per documenti standard con `MediaBox = [0 0 595 842]` questo non e' visibile.
Per documenti con `MediaBox = [36 72 595 842]` (margini non zero), le coordinate sono errate.

**Soluzione**: in `MacPdfRenderer::getPage()`, convertire `mediaBox.origin.y` con
`pageHeight - origin.y - height` o, se si usa la rappresentazione di juce `(x, y, w, h)`,
assicurarsi che `y` sia l'angolo in alto a sinistra.

---

## Dipendenze per piattaforma

| Piattaforma | Backend | Librerie richieste | File in third_party | Setup necessario |
|---|---|---|---|---|
| macOS | CoreGraphics | Framework Apple (automatico) | - | Nessuno |
| iOS | CoreGraphics | Framework Apple (automatico) | - | Nessuno |
| Windows | PDFium | pdfium.dll, pdfium.lib | `pdfium/win/` | `setup_pdfium.ps1` |
| Linux | PDFium | libpdfium.so | `pdfium/linux/` | `setup_pdfium.sh` |
| Android | PDFium | libpdfium.so (per ABI) | `pdfium/android/<abi>/` | `setup_pdfium.sh` |

### Note deployment

- **macOS**: `libpdfium.dylib` presente in `third_party/pdfium/mac/` (installata da `setup_pdfium.sh`).
  La dylib NON viene usata dal backend macOS (che usa CoreGraphics). E' disponibile per test futuri
  o per un eventuale utilizzo ibrido. Il backend macOS non dipende da PDFium.
- **Windows**: `pdfium.dll` deve essere distribuita accanto al `.vst3` o nel PATH di sistema.
  Il linker richiede `pdfium.lib` a compile-time, la DLL a runtime.
- **Linux**: `libpdfium.so` deve essere nella stessa directory del `.so` del plugin o in `LD_LIBRARY_PATH`.
  Usare `-Wl,-rpath,$$ORIGIN` per embedding nel pacchetto.
- **Android**: una `.so` per ogni ABI (arm64-v8a, armeabi-v7a, x86, x86_64) deve essere nel bundle APK.
