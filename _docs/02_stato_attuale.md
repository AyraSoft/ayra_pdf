# Stato Attuale — ayra_pdf

**Autore**: Ayra Soft  
**Data aggiornamento**: 2026-10-02

---

> **AUTORITA' DEL DOCUMENTO** — Questo file e' la **Single Source of Truth** per stato,
> priorita', dipendenze e avanzamento del refactoring `ayra_pdf`. I documenti
> `03_fase2_...` ... `06_fase5_...` sono specifiche tecniche di area e NON possiedono
> lo stato operativo. `00_storico.md` e' provenance storica.
>
> **Guideline normativa** — Ogni modifica segue
> `AyraSoft/ayra_multiverse/_doc/AYRA_AI_ENGINEERING_MASTER.md`: correctness, SSoT,
> module-first, current-only, minimal architectural diff, repository safety e verification gate.
>
> **Limite di questa sessione** — si possono leggere/scrivere codice e documentazione,
> ma **non si possono lanciare build, test binari o CI**. Ogni microstep distingue quindi
> `static-audited` da `externally-verified`.

## Baseline verificata

Ricognizione effettuata su `main@fc60d44e325265b85a1616205157ee871d95350a` prima
dell'avvio dell'implementazione del 2026-10-02.

- L'architettura v2 (engine / renderer / widget) esiste come skeleton.
- Il solo path legacy operativo e' macOS/AppKit (`PDFView` deriva da `NSView`); non esiste un legacy iOS funzionante.
- `MacPdfRenderer` M01-M04 e' implementato staticamente; `PdfiumRenderer`, `PdfDocument` e gran parte di `PdfViewComponent` hanno ancora lavoro pending.
- Code search AyraSoft non ha trovato consumer di `PDFComponent` fuori da questo repository;
  l'audit va ripetuto immediatamente prima del cutover.
- Il vecchio piano `using PDFComponent = PdfViewComponent` e' annullato: il Master impone current-only.
- `AYRA_PDF_HEADLESS` oggi esclude i widget, ma il modulo include/dichiara ancora `juce_gui_extra`;
  quindi il vecchio claim "zero dipendenze GUI" non e' attualmente vero.

## Roadmap canonica a microstep

| ID | Area | Deliverable | Dipendenze | Stato |
|---|---|---|---|---|
| **S00** | Governance/docs | Stato canonico, roadmap, current-only, limiti verifica | - | **done (static)** |
| **M01** | Mac renderer | Lifecycle, load file/memory, close, page count/metadata | S00 | **done (static)** |
| **M02** | Mac renderer | Rasterizzazione pagina -> `juce::Image` con validazione scale/size | M01 | **done (static)** |
| **M03** | Mac renderer | Save file + save memory byte-preserving | M01 | **done (static)** |
| **M04** | Mac renderer | Estrazione testo + ricerca PDFKit con bounds esatti | M01 | **done (static)** |
| **D01** | Engine | `PdfDocument` factory/delega attiva su Apple | M01-M04 | **in progress** |
| **P01** | PDFium | Init/lifetime, load file/memory, close, metadata | S00 | pending |
| **P02** | PDFium | Render + save file/memory | P01 | pending |
| **P03** | PDFium | Extract/search con bounds esatti | P01 | pending |
| **D02** | Engine | `PdfDocument` completo su tutti i target dichiarati | D01,P01-P03 | pending |
| **W01** | Widget | Load/navigation/cache/render + notifiche | D02 | pending |
| **W02** | Widget | Zoom anchor, pan, wheel/pinch, HiDPI, search overlay | W01 | pending |
| **H01** | Module contract | Rendere esplicito e corretto il contratto headless/dependencies JUCE | D02 | pending |
| **C01** | Current-only cutover | Rimuovere `pdf_component/`, include legacy e ogni alias/fallback | W02,H01 | pending |
| **V01** | Verification esterna | Build/test target dichiarati + sanitizer dove applicabile | C01 | external pending |

### Regole di avanzamento

1. Un microstep cambia solo il canonical owner della responsabilita' coinvolta.
2. Il legacy e' reference read-only, non una seconda implementazione da mantenere sincronizzata.
3. Una funzione completata perde il relativo `jassertfalse`/TODO; niente placeholder mascherati.
4. PDF, indici, size e path sono input non affidabili: validare prima di allocare/indicizzare.
5. Render/parse/save non sono realtime-safe e non vanno chiamati dal processBlock.
6. Nessun `PdfSearchResult` con bounds finti/vuoti viene dichiarato completo.
7. Fine microstep: diff audit statico su correctness, ownership, lifetime, scope, regressioni.
8. Build/test/CI rimangono **external pending** in questa sessione.

## Change Contract — M01 MacPdfRenderer lifecycle/load/metadata

**Task model**: rendere reale il possesso del documento CoreGraphics senza introdurre stato viewport/GUI.

**Canonical owner**: `renderer/mac/ayra_MacPdfRenderer.mm` possiede `CGPDFDocumentRef` e
l'eventuale backing memory necessaria per un documento aperto da buffer.

**Existing reusable path**: il legacy `MacPDFComponent.mm` e' solo riferimento read-only;
bug e ownership legacy non vengono copiati automaticamente.

**Expected files**:
- `renderer/mac/ayra_MacPdfRenderer.mm` — implementazione;
- `renderer/mac/ayra_MacPdfRenderer.h` — solo allineamento del contratto/documentazione;
- questo file — stato a fine microstep.

**Forbidden ownership changes**:
- nessun zoom/pan/current-page nel renderer;
- nessuna logica widget in engine/renderer;
- nessuna modifica a JUCE o third-party;
- nessun alias/compatibility layer.

**Invariants**:
- `close()` idempotente e unico punto di rilascio del documento;
- ogni successful load sostituisce deterministicamente il documento precedente;
- backing buffer del load-from-memory vivo almeno quanto il documento;
- API pagina 0-based, CoreGraphics 1-based confinato all'adapter;
- indice negativo/out-of-range controllato prima di cast unsigned;
- nessun leak di documento/provider; nessun accesso GUI.

**Acceptance criteria M01**:
- `loadFromFile`, `loadFromMemory`, `close`, `isLoaded`, `getPageCount`, `getPage` reali;
- nessun `jassertfalse` in tali funzioni;
- input invalidi non lasciano stato parzialmente caricato;
- `getPage` ritorna indice, bounds e rotazione coerenti;
- static audit completato; build/test **external pending**.

**Esito M01 (2026-10-02)**: implementati lifecycle transazionale, ownership esplicita
`CGPDFDocumentRef -> CGDataProviderRef -> backing MemoryBlock`, caricamento file/memoria,
conteggio e metadati pagina con range/narrowing checks. Nessuna build/CI eseguita qui.

---

## Esito M02 / M03

**M02 (2026-10-02)**: raster CoreGraphics diretto su `juce::SoftwareImageType`,
ARGB premultiplied compatibile col layout JUCE Apple, backdrop bianco, output top-down,
rotazioni PDF 0/90/180/270 gestite dal transform CoreGraphics. Input `scale`, dimensioni,
pixel count, stride e buffer size sono bounded/validati. Limiti canonici condivisi con il
futuro backend PDFium in `renderer/ayra_PdfSafetyLimits.h`.

**M03 (2026-10-02)**: `saveToFile` e `saveToMemory` sono byte-preserving tramite snapshot
del `CGDataProvider`; non ridisegnano le pagine e quindi non appiattiscono metadata,
annotations, form fields o struttura. Le destinazioni hanno semantica transazionale.
Build e round-trip runtime restano **external pending**.

## Change Contract — M04 text extraction / search Apple

**Task model**: fornire estrazione testo Unicode e ricerca con bounds reali senza implementare
un parser PDF/font/CMap proprietario.

**Canonical owner**:
- `MacPdfRenderer`: orchestration del backend Apple;
- CoreGraphics: lifecycle/raster;
- PDFKit: content model testuale e selezioni;
- `PdfSearchResult`: contratto geometrico cross-backend.

**Existing reusable path**: PDFKit `PDFDocument/PDFPage/PDFSelection`. Il vecchio design
`CGPDFScanner` manuale e' scartato perche' non copre in modo robusto Type0/CMap/XObject e
non garantisce bounds corretti.

**Expected files**:
- `ayra_pdf.h` — dichiarazione framework PDFKit per macOS/iOS;
- `renderer/mac/ayra_MacPdfRenderer.mm` — cache PDFKit derivata dai byte canonici;
- `engine/ayra_PdfSearchResult.h`, `PdfPage.h`, `PdfRenderer.h`, `PdfDocument.h` — contratto;
- documentazione corrente.

**Forbidden ownership changes**:
- nessun testo/search nel widget;
- nessun secondo buffer PDF editabile/autoritativo;
- nessun parser CMap/font custom;
- nessuna conversione in screen space nel renderer.

**Invariants**:
- il modello PDFKit e' una cache derivata dagli stessi byte del provider CoreGraphics;
- invalidazione deterministica in `close()`/nuovo load;
- `findText` e' case-insensitive su tutti i backend;
- bounds sempre 72 dpi, lower-left/Y-up; il widget converte una sola volta;
- `pageIndex` -1 = documento intero, altrimenti 0-based e validato prima di chiamare PDFKit;
- risultati senza bounds finite/non-empty non vengono pubblicati;
- parser/search non sono realtime-safe e l'istanza resta externally serialized;
- eccezioni native da PDF esterni malformati vengono tradotte in failure/risultato vuoto al boundary.

**Acceptance criteria M04**:
- `extractText` usa `PDFPage.string` e preserva Unicode;
- `findText` cerca per pagina su `PDFPage.string`, converte ogni range con `selectionForRange:` e usa `PDFSelection boundsForPage:`;
- nessun `jassertfalse` resta nei metodi Mac;
- nessun bounds placeholder;
- static diff audit completato;
- test runtime su UTF-8/UTF-16, Type0/CMap, multiline match, pagina filtrata e PDF malformato:
  **external pending**.

---

## Esito M04

**M04 (2026-10-02)**: il backend Apple usa PDFKit come content model derivato dai medesimi
byte del `CGDataProvider`. `extractText` usa `PDFPage.string`; `findText` e'
case-insensitive, page-bounded, converte i range tramite `selectionForRange:` e pubblica
solo bounds finite/non-empty in PDF page space. Query, testo per pagina e numero di match
sono bounded. Superato il budget risultati, la ricerca fallisce atomicamente: nessuna lista
troncata viene presentata come completa. La cache PDFKit viene invalidata prima di
document/provider/backing. Nel renderer Apple non restano `jassertfalse` o TODO.
Build/test runtime restano **external pending**.

### Nota piattaforma Apple

Il backend v2 e' scritto sotto `JUCE_MAC || JUCE_IOS` e usa framework disponibili su entrambe
le piattaforme. Il legacy invece e' AppKit/macOS-specifico (`PDFView : NSView`): finche' C01
non rimuove quel percorso, l'integrazione iOS dell'intero modulo NON e' dichiarata verificata.

## Change Contract — D01 PdfDocument facade Apple

**Task model**: trasformare `PdfDocument` da skeleton in factory/facade reale, senza spostare
nel facade alcuna logica PDF.

**Canonical owner**:
- `PdfDocument`: ownership del `std::unique_ptr<PdfRenderer>` e sola delega;
- `MacPdfRenderer`: tutta la semantica Apple;
- `PdfiumRenderer`: semantica non-Apple, ancora pending P01-P03.

**Expected files**:
- `engine/ayra_PdfDocument.cpp`;
- documentazione D01;
- commenti di aggregazione in `ayra_pdf.cpp` solo se necessari.

**Forbidden ownership changes**:
- nessun parsing/raster/search nel facade;
- nessuna duplicazione di safety limit o policy;
- nessun fallback legacy;
- nessuna modifica a JUCE/third-party.

**Invariants**:
- factory compile-time: Apple -> `MacPdfRenderer`, Win/Linux/Android -> `PdfiumRenderer`;
- ogni metodo pubblico e' guard + delega, nessuna seconda source of truth;
- nessun `jassertfalse`/TODO nel facade;
- nessuna allocazione o chiamata PDF e' realtime-safe;
- D01 certifica staticamente il percorso Apple; D02 resta bloccato da P01-P03.

**Acceptance criteria D01**:
- costruttore istanzia il renderer corretto;
- open/save/close/state/page/render/search/extract delegano 1:1;
- `getRenderer()` resta accesso non-owning avanzato;
- diff audit statico completato;
- build/test macOS+iOS: **external pending**.

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
| `renderer/mac/ayra_MacPdfRenderer.mm` | ✅ M01-M04 completi staticamente | macOS/iOS | 2 |
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

**Soluzione v2**: `MacPdfRenderer::getPage()` mantiene volutamente le bounds canoniche in PDF page space (72 dpi, lower-left/Y-up). La conversione verso coordinate JUCE/schermo appartiene esclusivamente al widget, evitando doppie conversioni.

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
