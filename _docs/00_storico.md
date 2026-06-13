# Storico — ayra_pdf

**Autore**: Ayra Soft  
**Data creazione**: 2026-05-26  
**Stato**: refactoring attivo (v2.0 in costruzione, v1 legacy ancora attivo)

---

## Contesto iniziale (v1 — struttura monolitica)

Il modulo nella sua prima versione era organizzato attorno a una singola classe facade `PDFComponent`
(in `pdf_component/PDFComponent.h`) che delegava il rendering a implementazioni piattaforma-specifiche
selezionate via `#if JUCE_MAC || JUCE_IOS / JUCE_WINDOWS / JUCE_ANDROID || JUCE_LINUX`.

### macOS/iOS — funzionante

Il backend macOS era composto da due file Objective-C:

- `pdf_component/Mac_PDF_core/PDFView.m` — NSView custom che wrappa `CGPDFDocumentRef`, gestisce zoom,
  pan e rendering tramite `drawRect:`. Usa `CGPDFPageGetDrawingTransform` + `CGContextDrawPDFPage`.
- `pdf_component/Mac_PDF_core/MacPDFComponent.mm` — classe C++/ObjC `MacPDFViewComponent` che alloca
  `PDFView`, espone l'API richiesta da `PDFComponent` (load, zoom, pan, export, page navigation).

Il rendering era funzionante ma con diversi bug (vedi sezione Bug noti).

### Windows — stub con rendering finto

`WindowsPDFViewComponent` usava la libreria Win32 `PdfLib` (o variante proprietaria locale), con
un'implementazione parziale che disegnava rettangoli grigi come placeholder delle pagine. Il conteggio
pagine era approssimato contando le occorrenze della stringa `/Page` nel buffer grezzo del file.
Hardcoded A4 size (`595 x 842` punti) per qualsiasi documento.

### Linux — stub con dipendenza fragile

`LinuxPDFViewComponent` lanciava `pdftocairo` come subprocess via `system()` con la path del file
interpolata direttamente nella stringa del comando — vulnerabile a shell injection se la path contiene
caratteri speciali (spazi, virgolette, dollaro, backtick). Il rendering produceva un PNG temporaneo
letto in memoria, senza gestire cleanup in caso di errore o interruzione del processo.

---

## Bug noti v1 (ordinati per severita')

### 1. Shell injection in LinuxPDFViewComponent (severita': critica)

```cpp
// v1 — vulnerabile
std::string cmd = "pdftocairo -png " + filePath + " /tmp/ayra_pdf_tmp";
system(cmd.c_str());
```

Se `filePath` contiene `; rm -rf ~` o simili, il comando viene eseguito.
Fix: usare `execvp` con array di argomenti separati, oppure PDFium che non lancia subprocess.

### 2. getMemoryBlockFromDocument rotto (severita': alta)

```objc
// v1 — MacPDFComponent.mm:256-282
CGContextRef pdfContext = CGPDFContextCreateWithURL(
    (__bridge CFURLRef)[NSURL URLWithString:@"dummyURL"], NULL, NULL);
```

`CGPDFContextCreateWithURL` con `@"dummyURL"` (non un URL file valido) produce un contesto
invalido: l'output non viene scritto da nessuna parte. La funzione ritorna sempre un
`MemoryBlock` corrotto (`sizeof(CGPDFDocumentRef)` byte di junk).

### 3. Zoom anchor point formula errata (severita': media)

```cpp
// v1 — MacPDFComponent.mm:160-163 (setCurrentPageZoom)
float xOffset = handlePoint.getX() - (handlePoint.getX() * pdfView.zoomLevel);
float yOffset = handlePoint.getY() - (handlePoint.getY() * pdfView.zoomLevel);
pdfView.topLeftOrigin = CGPointMake(origin.getX() + xOffset, origin.getY() + yOffset);
```

Il calcolo moltiplica il punto di handle per il **nuovo** zoomLevel gia' applicato, ignorando
il vecchio zoom. Il risultato e' che lo zoom ancora point e' sempre relativo all'origine 0,0
indipendentemente da dove si trova il mouse. La pagina salta visibilmente ad ogni operazione
di zoom con pinch o scroll.

### 4. Conteggio pagine approssimato su Windows/Linux (severita': bassa)

Il parser grezzo conta le occorrenze di `/Page` nel buffer del file. Documenti con metadati,
thumbnail embedded, o form fields che menzionano `/Page` producono un conteggio errato.

### 5. getCurrentPageBounds su MacPDFViewComponent — origine CoreGraphics non convertita

```cpp
// v1 — MacPDFComponent.mm:177-183
return juce::Rectangle<float> {
    (float)mediaBox.origin.x,     // Y CoreGraphics = Y-up, juce = Y-down
    (float)mediaBox.origin.y,     // origine non convertita
    ...
};
```

Per documenti con `MediaBox` non in origine (es. documenti scansionati ritagliati), la bounding
box restituita ha coordinate errate nel sistema di riferimento juce.

---

## Analisi librerie PDF — decisione architetturale

La specifica PDF (ISO 32000) e' documentata in oltre 1.000 pagine e copre: encoding font (Type1,
TrueType, CIDFont, OpenType), color spaces (ICC profiles, CMYK, spot colors), transparency model
(alpha compositing, blend modes), form fields interattivi, firme digitali, JavaScript, XFA.
Scrivere da zero un renderer production-quality richiederebbe 18-24 mesi di lavoro.

### Librerie valutate

| Libreria | Licenza | Decisione | Motivazione |
|---|---|---|---|
| **PDFium** | Apache 2.0 | **SCELTO** per Win/Linux/Android | Battle-tested (Chrome, Flutter), precompilato, C API pulita, zero overhead di linking runtime |
| MuPDF | AGPL 3.0 + commercial | Scartato | Licenza commerciale obbligatoria per VST closed-source (costo elevato per plugin audio) |
| libpoppler | GPL 2.0 | Scartato | Copyleft incompatibile con distribuzione VST commerciale senza rilascio sorgenti |
| Header-only / light | - | Inesistente | Non esistono soluzioni production-quality header-only per PDF completo |
| **CoreGraphics** | Apple frameworks | **MANTENUTO** per macOS/iOS | Zero dipendenze, hardware-accelerated, gia' funzionante, qualita' rendering ottima |

### Perche' PDFium e non MuPDF

MuPDF e' tecnicamente eccellente ma la licenza AGPL richiede che qualsiasi software distribuito
che la include sia anch'esso rilasciato con sorgenti AGPL, oppure acquistata una licenza commerciale
da Artifex ($5.000-$20.000/anno per prodotto). Per plugin VST/AU distribuiti su store chiuso, questa
opzione e' incompatibile senza pagare per ogni prodotto.

PDFium (Apache 2.0) non ha tale vincolo: puo' essere linkato in qualsiasi prodotto, incluso software
commerciale closed-source, senza obblighi di rilascio sorgenti.

---

## Decisioni architetturali prese nel refactoring

### 1. Separazione engine / renderer / widgets

Aderenza al principio "server-side ready" di CLAUDE.md: il rendering puro (engine + renderer) non
dipende da juce GUI, puo' girare headless in CI, agenti AI, render offline.

```
engine/     <- PdfDocument (facade), PdfPage, PdfSearchResult — zero GUI
renderer/   <- PdfRenderer (interfaccia pura), MacPdfRenderer, PdfiumRenderer — zero GUI
widgets/    <- PdfViewComponent (juce::Component) — solo se !AYRA_PDF_HEADLESS
```

### 2. PdfRenderer — interfaccia pura

Permette di aggiungere nuovi backend (es. un futuro backend basato su Skia o su WebAssembly) senza
modificare PdfDocument o PdfViewComponent. La scelta del backend avviene a compile-time tramite
macro di piattaforma, non a runtime: zero overhead di vtable sul rendering path (il renderer e'
`final` e il compilatore puo' devirtualizzare grazie a `unique_ptr<ConcreteType>`).

### 3. Backward compatibility — alias PDFComponent

`PDFComponent` rimane attivo tramite `pdf_component/PDFComponent.h` fino al completamento della
Fase 5. In Fase 5, il file viene rimosso e sostituito dall'alias:
```cpp
namespace ayra { using PDFComponent = PdfViewComponent; }
```
Le firme pubbliche di `PdfViewComponent` sono identiche a quelle di `PDFComponent` per garantire
zero modifiche nei siti di chiamata esistenti.

### 4. PDFium binari fuori dal repository git

I binari precompilati (`.dylib`, `.dll`, `.so`) pesano 10-30 MB ciascuno e renderebbero il
repository inutilizzabile. Gli **header** sono in git (necessari per la compilazione), i binari
vengono scaricati tramite script (`scripts/setup_pdfium.sh` / `setup_pdfium.ps1`).

### 5. Headless mode via AYRA_PDF_HEADLESS

Definendo `AYRA_PDF_HEADLESS` prima dell'inclusione del modulo, il blocco `widgets/` viene
escluso. L'engine e i renderer rimangono compilabili senza juce_gui_basics/juce_gui_extra.

---

## Lavoro svolto nel refactoring (maggio 2026)

### 1. Analisi stato v1

Lettura completa di tutti i file legacy: `PDFComponent.h/cpp`, `MacPDFComponent.mm`, `PDFView.m`,
stub Windows e Linux. Identificazione dei bug e delle limitazioni architetturali.

### 2. Ricerca librerie e decisione su PDFium

Confronto licenze (Apache 2.0 vs AGPL vs GPL vs commercial). Verifica compatibilita' con modello
di distribuzione VST commerciale chiuso. Decisione documentata nella tabella sopra.

### 3. Piano di lavoro in 7 fasi

Definizione delle fasi con dipendenze, prerequisiti e criteri di completamento:
- Fase 1: skeleton struttura (completata)
- Fase 2: MacPdfRenderer CoreGraphics (in attesa)
- Fase 3: PdfiumRenderer PDFium Win/Linux (in attesa)
- Fase 4: PdfDocument collegamento renderer (in attesa — dipende da Fase 2 e 3)
- Fase 5: PdfViewComponent implementazione completa (in attesa — dipende da Fase 4)
- Fase 6: Rimozione legacy pdf_component/ (in attesa — dipende da Fase 5)
- Fase 7: LookAndFeel finale e polish (opzionale)

### 4. Creazione struttura cartelle skeleton

```
ayra_pdf/
├── engine/          <- PdfDocument.h/cpp, PdfPage.h, PdfSearchResult.h
├── renderer/
│   ├── mac/         <- MacPdfRenderer.h/.mm
│   └── pdfium/      <- PdfiumRenderer.h/.cpp
├── widgets/
│   └── look_and_feel/  <- LookAndFeelMethods.h, DefaultLookAndFeel.h/.cpp
├── third_party/
│   └── pdfium/
│       └── include/ <- header PDFium (in git)
└── scripts/         <- setup_pdfium.sh, setup_pdfium.ps1
```

### 5. Scrittura file skeleton con interfacce complete

Tutti gli header con firme complete e documentazione Doxygen. Implementazioni con `jassertfalse`
e commenti TODO per ogni metodo da completare.

### 6. Scrittura README.md e SETUP.md

Documentazione dell'API pubblica, guida all'installazione PDFium, configurazione Projucer/CMake.

### 7. Script setup_pdfium.sh e setup_pdfium.ps1

Script bash per macOS/Linux e PowerShell per Windows che:
- Scaricano i binari da `bblanchon/pdfium-binaries` (GitHub releases)
- Costruiscono fat binary (arm64 + x86_64) su macOS con `lipo`
- Posizionano i file nella struttura attesa dal modulo

### 8. Esecuzione script — download PDFium chromium/7857

Download effettuato con successo. Fat binary macOS: `arm64` + `x86_64` fusi con `lipo`.
Risultato: `libpdfium.dylib` (~14 MB) in `third_party/pdfium/mac/`.

### 9. Fix bug script — ANSI color codes nel VERSION_ENCODED

Bug: la funzione `log()` dello script usava `echo` con codici ANSI all'interno di una
command substitution, facendo si' che i codici di colore finissero nella variabile
`VERSION_ENCODED` usata per costruire l'URL di download. Il server GitHub restituiva 404.

Fix: separazione netta tra funzioni di logging (stdout con ANSI) e funzioni di costruzione
URL (output puro senza ANSI).

### 10. Fix supporto .dylib invece di .a su macOS

A partire da `bblanchon/pdfium-binaries` >= `chromium/7000`, i release per macOS distribuiscono
`.dylib` (shared library) invece di `.a` (static archive). Lo script e' stato aggiornato per
rilevare automaticamente quale formato e' disponibile per la versione richiesta e usare `lipo`
su entrambi i formati.

---

## Note importanti emerse dal processo

### .dylib vs .a su macOS

Versioni PDFium `< chromium/7000`: `.a` (statica) — linkata nel binario VST, zero deployment extra.
Versioni `>= chromium/7000`: `.dylib` (dinamica) — deve essere bundled nel pacchetto del plugin.

La versione `chromium/7857` (installata) e' `.dylib`. Questo implica che per distribuire il plugin
su macOS e' necessario includere `libpdfium.dylib` nel bundle `.vst3/Contents/MacOS/`.

### Fat binary e lipo

`lipo -create libpdfium-arm64.dylib libpdfium-x86_64.dylib -output libpdfium.dylib` funziona sia
per `.a` che per `.dylib`. Il fat binary risultante funziona su Mac Intel e Apple Silicon.

### RPATH per dylib nel bundle VST3

Il binario del plugin deve essere linkato con:
```
-Wl,-rpath,@loader_path
```
e `libpdfium.dylib` deve trovarsi nella stessa directory del binario del plugin
(`Contents/MacOS/`). Vedi `07_deployment.md` per la procedura completa.
