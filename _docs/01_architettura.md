# Architettura — ayra_pdf

**Autore**: Ayra Soft  
**Data creazione**: 2026-05-26

---

## Panoramica layer

```
ayra_pdf/
|
+-- engine/                     <- LAYER 1: logica pura, headless-safe
|   +-- ayra_PdfPage.h          <- struct PdfPage (valore, immutabile)
|   +-- ayra_PdfSearchResult.h  <- struct PdfSearchResult (valore, immutabile)
|   +-- ayra_PdfDocument.h      <- class PdfDocument (facade cross-platform)
|   +-- ayra_PdfDocument.cpp    <- implementazione facade + factory renderer
|
+-- renderer/                   <- LAYER 2: backend di rendering (headless-safe)
|   +-- ayra_PdfRenderer.h      <- class PdfRenderer (interfaccia pura)
|   +-- mac/
|   |   +-- ayra_MacPdfRenderer.h    <- CoreGraphics (macOS/iOS)
|   |   +-- ayra_MacPdfRenderer.mm   <- implementazione ObjC/CoreGraphics
|   +-- pdfium/
|       +-- ayra_PdfiumRenderer.h    <- PDFium Apache 2.0 (Win/Linux/Android)
|       +-- ayra_PdfiumRenderer.cpp  <- implementazione C++/PDFium
|
+-- widgets/                    <- LAYER 3: GUI (escluso se AYRA_PDF_HEADLESS)
|   +-- ayra_PdfViewComponent.h      <- class PdfViewComponent (juce::Component)
|   +-- ayra_PdfViewComponent.cpp    <- implementazione widget
|   +-- look_and_feel/
|       +-- ayra_PdfLookAndFeelMethods.h      <- interfaccia LAF aggregata
|       +-- ayra_PdfDefaultLookAndFeel.h/.cpp <- implementazione LAF default
|
+-- pdf_component/              <- LAYER LEGACY (rimosso in Fase 6)
|   +-- PDFComponent.h/.cpp          <- facade v1 (deprecated)
|   +-- Mac_PDF_core/
|       +-- PDFView.m                <- NSView custom (renderizza pagina)
|       +-- MacPDFComponent.mm       <- MacPDFViewComponent (wrapper ObjC)
|
+-- third_party/
|   +-- pdfium/
|       +-- include/            <- header PDFium (in git)
|       +-- mac/                <- libpdfium.dylib (NON in git, setup_pdfium.sh)
|       +-- win/                <- pdfium.dll + pdfium.lib (NON in git)
|       +-- linux/              <- libpdfium.so (NON in git)
|
+-- scripts/
|   +-- setup_pdfium.sh         <- download e setup binari macOS/Linux
|   +-- setup_pdfium.ps1        <- download e setup binari Windows
|
+-- _docs/                      <- questa cartella
+-- ayra_pdf.h                  <- root header del modulo
+-- ayra_pdf.cpp                <- root implementation del modulo
```

### Layer 1 — Engine (headless-safe)

Dipende solo da `juce_core` e `juce_audio_basics`. Nessuna GUI. Usabile da:
- Thread audio (solo lettura di stato — `getPageCount`, `getPage`)
- Agenti AI server-side senza display
- Test unitari headless in CI

`PdfDocument` e' la classe che il consumer usa direttamente nell'applicazione.
Internamente possiede un `unique_ptr<PdfRenderer>` con il backend corretto.

### Layer 2 — Renderer (headless-safe)

`PdfRenderer` e' un'interfaccia pura con due implementazioni concrete:
- `MacPdfRenderer`: usa `CGPDFDocumentRef` (CoreGraphics, framework Apple). Solo macOS/iOS.
- `PdfiumRenderer`: usa `FPDF_DOCUMENT` (PDFium, C API). Windows, Linux, Android.

Entrambe usano il pattern Pimpl per tenere i tipi ObjC/PDFium fuori dagli header C++.
Questo e' necessario perche' `CGPDFDocumentRef` e' un tipo CoreGraphics che non puo' comparire
in un header incluso da codice C++ puro, e `FPDF_DOCUMENT` analogamente richiederebbe gli
header PDFium presenti ovunque il renderer fosse incluso.

### Layer 3 — Widgets (GUI, escluso in headless mode)

`PdfViewComponent` e' un `juce::Component` che:
- Possiede un `PdfDocument` interno (`ownedDocument`) per il caso "il widget carica il file"
- Punta a un `PdfDocument*` esterno opzionale (`currentDocument`) per il caso "documento condiviso"
- Implementa il Widget Pattern Ayra completo: `ColourIds`, `LookAndFeelMethods`, `Listener` + `std::function`

---

## Pattern PdfRenderer — interfaccia pura

```cpp
class PdfRenderer
{
public:
    virtual ~PdfRenderer() = default;

    [[nodiscard]] virtual bool        loadFromFile   (const juce::File&)             noexcept = 0;
    [[nodiscard]] virtual bool        loadFromMemory (const void*, size_t)           noexcept = 0;
    [[nodiscard]] virtual bool        saveToFile     (const juce::File&)       const noexcept = 0;
    [[nodiscard]] virtual bool        saveToMemory   (juce::MemoryBlock&)      const noexcept = 0;
    virtual void                      close          ()                              noexcept = 0;
    [[nodiscard]] virtual bool        isLoaded       ()                        const noexcept = 0;
    [[nodiscard]] virtual int         getPageCount   ()                        const noexcept = 0;
    [[nodiscard]] virtual PdfPage     getPage        (int pageIndex)           const noexcept = 0;
    [[nodiscard]] virtual juce::Image renderPage     (int pageIndex, float scale)   noexcept = 0;
    [[nodiscard]] virtual juce::Array<PdfSearchResult> findText  (const juce::String&, int) noexcept = 0;
    [[nodiscard]] virtual juce::String                 extractText (int pageIndex)  noexcept = 0;
};
```

**Perche' l'interfaccia pura**: permette di aggiungere nuovi backend (Skia, WebKit, PoDoFo)
senza modificare `PdfDocument` o `PdfViewComponent`. Il consumer (PdfDocument) non conosce quale
renderer sta usando — lavora solo sull'interfaccia.

**Perche' NON runtime polymorphism per la scelta del renderer**: la selezione del renderer avviene
tramite `#if` a compile-time nel costruttore di `PdfDocument`. Non esiste un meccanismo a runtime
per cambiare backend (non ha senso: la piattaforma e' fissa a compile-time). Il vantaggio e' che il
compilatore puo' devirtualizzare le chiamate al renderer conoscendo il tipo concreto via `unique_ptr`.

---

## Pattern PdfDocument — facade

```cpp
// In ayra_PdfDocument.cpp (Fase 4):
PdfDocument::PdfDocument()
{
#if JUCE_MAC || JUCE_IOS
    renderer = std::make_unique<MacPdfRenderer>();
#elif JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID
    renderer = std::make_unique<PdfiumRenderer>();
#else
    #error "ayra_pdf: piattaforma non supportata"
#endif
}
```

Tutti i metodi pubblici di `PdfDocument` delegano al renderer con un guard su `renderer != nullptr`:

```cpp
bool PdfDocument::open (const juce::File& file) noexcept
{
    return renderer && renderer->loadFromFile (file);
}
int PdfDocument::getPageCount() const noexcept
{
    return renderer ? renderer->getPageCount() : 0;
}
```

L'unica logica propria di `PdfDocument` e' la factory nel costruttore e i guard.

---

## Pattern PdfViewComponent — widget

### Doppia ownership del documento

```
PdfViewComponent
  |
  +-- ownedDocument   (PdfDocument, membro diretto)
  +-- currentDocument (PdfDocument*, puntatore — non-owning)
```

- `loadDocument(path)` e `loadDocumentFromMemoryBlock(data, size)` caricano in `ownedDocument`
  e impostano `currentDocument = &ownedDocument`.
- `setDocument(PdfDocument& doc)` imposta `currentDocument = &doc` senza toccare `ownedDocument`.
  Il chiamante possiede il documento e deve garantirne la vita per tutta la durata dell'uso.

### Widget Pattern Ayra rispettato

```
ColourIds:
  backgroundColourId     = 0x3AB0001
  pageColourId           = 0x3AB0002
  shadowColourId         = 0x3AB0003
  noDocumentTextColourId = 0x3AB0004

LookAndFeelMethods:
  drawPdfViewBackground  (g, w, h, comp)
  drawPdfViewNoDocument  (g, w, h, comp)
  drawPdfViewPageShadow  (g, pageBounds, comp)

Listener:
  pdfPageChanged   (comp, newPage)
  pdfDocumentLoaded(comp)
  pdfDocumentClosed(comp)

std::function:
  onPageChanged    -> void(int)
  onDocumentLoaded -> void()
  onDocumentClosed -> void()
```

---

## Backward compatibility

### Stato attuale (Fasi 1-4 incomplete)

`PDFComponent` legacy e' ancora l'unica implementazione funzionante su macOS.
`PdfViewComponent` e i renderer sono skeleton con `jassertfalse` nei metodi non ancora implementati.

**Inclusione corrente in ayra_pdf.h:**
```cpp
#include "pdf_component/PDFComponent.h"   // LEGACY, ancora attivo
// using PDFComponent = PdfViewComponent; // commentato — attivare in Fase 5
```

**Inclusione corrente in ayra_pdf.cpp:**
```cpp
#include "pdf_component/Mac_PDF_core/PDFView.m"
#include "pdf_component/Mac_PDF_core/MacPDFComponent.mm"
#include "renderer/mac/ayra_MacPdfRenderer.mm"  // skeleton
// ...
#include "pdf_component/PDFComponent.cpp"       // LEGACY ancora incluso
```

### Piano di migrazione

| Fase | Azione | Quando |
|---|---|---|
| Fase 2 | MacPdfRenderer completato | Prima priorita' macOS |
| Fase 3 | PdfiumRenderer completato | Priorita' Win/Linux |
| Fase 4 | PdfDocument collegato ai renderer | Dipende da 2 e 3 |
| Fase 5 | PdfViewComponent completo, alias attivato | Dipende da 4 |
| Fase 6 | `pdf_component/` eliminata | Dopo verifica in produzione |

### Come attivare l'alias (Fase 5)

In `ayra_pdf.h`:
1. Commentare `#include "pdf_component/PDFComponent.h"`
2. Decommentare `namespace ayra { using PDFComponent = PdfViewComponent; }`

In `ayra_pdf.cpp`:
1. Rimuovere include di `PDFView.m`, `MacPDFComponent.mm`, `PDFComponent.cpp`

---

## Headless mode

```cpp
// Prima dell'inclusione del modulo:
#define AYRA_PDF_HEADLESS
#include <ayra_pdf/ayra_pdf.h>
```

Con questa define, `ayra_pdf.h` esclude l'intero blocco `widgets/`:
```cpp
#ifndef AYRA_PDF_HEADLESS
  #include "widgets/ayra_PdfViewComponent.h"
  #include "widgets/look_and_feel/ayra_PdfLookAndFeelMethods.h"
  #include "widgets/look_and_feel/ayra_PdfDefaultLookAndFeel.h"
#endif
```

**Chi usa la modalita' headless:**
- Build CI senza display (rendering di test)
- Agenti AI che estraggono testo o metadati da PDF programmaticamente
- Server-side renderer che producono immagini senza GUI
- Tool di validazione documenti

**Cosa rimane disponibile in headless:**
- `PdfDocument` con tutti i metodi (open, close, getPage, renderPage, findText, extractText)
- `PdfPage`, `PdfSearchResult`
- `PdfRenderer` e le implementazioni concrete

**Cosa viene escluso:**
- `PdfViewComponent` (richiede juce_gui_basics)
- `PdfLookAndFeelMethods`, `PdfDefaultLookAndFeel`
- Tutto il blocco legacy `pdf_component/` (anch'esso GUI)

---

## PDFium — note di integrazione

### Init/destroy singleton

```cpp
// PDFium NON e' thread-safe e richiede init/destroy una volta per processo.
// Pattern consigliato con std::call_once:

namespace {
    std::once_flag   g_initFlag;
    std::atomic<int> g_instanceCount { 0 };
}

PdfiumRenderer::PdfiumRenderer() {
    std::call_once (g_initFlag, [] {
        FPDF_LIBRARY_CONFIG cfg;
        cfg.version          = 2;
        cfg.m_pUserFontPaths = nullptr;
        cfg.m_pIsolate       = nullptr;
        cfg.m_v8EmbedderSlot = 0;
        FPDF_InitLibraryWithConfig (&cfg);
    });
    g_instanceCount.fetch_add (1, std::memory_order_relaxed);
}

PdfiumRenderer::~PdfiumRenderer() {
    close();
    if (g_instanceCount.fetch_sub (1, std::memory_order_acq_rel) == 1)
        FPDF_DestroyLibrary();
}
```

### Thread safety

La documentazione PDFium afferma esplicitamente che nessuna API e' thread-safe.
Ogni `FPDF_DOCUMENT` deve essere usato da un singolo thread alla volta.
`PdfiumRenderer` non e' thread-safe — il consumer (PdfDocument, PdfViewComponent) deve
serializzare gli accessi se il renderer viene usato da thread multipli.

In pratica: `renderPage` e `extractText` vengono chiamati dal message thread (o da un
worker thread dedicato), mai dall'audio thread. Non serve mutex se il consumer e' single-thread.

### .dylib vs .a — quale release ha cosa

| Release PDFium | macOS formato | Note |
|---|---|---|
| < chromium/7000 | `.a` (static) | Linkata nel binario — niente deployment extra |
| >= chromium/7000 | `.dylib` (shared) | Deve essere bundled nel pacchetto plugin |
| chromium/7857 (installata) | `.dylib` | Fat binary arm64+x86_64 (~14 MB) |

Per iOS e Android: sempre `.a` statica (linker lo richiede per sicurezza delle app).

### Aggiornare la versione PDFium

1. Modificare `PDFIUM_VERSION` in `scripts/setup_pdfium.sh`
2. Eseguire `./scripts/setup_pdfium.sh` — sovrascrive i binari in `third_party/pdfium/`
3. Verificare che l'API pubblica non sia cambiata (raro tra versioni minori di chromium)
4. Ricompilare il modulo e verificare che i test di rendering producano output coerente
5. Controllare il CHANGELOG di `bblanchon/pdfium-binaries` per breaking changes documentati
