# ayra_pdf

Modulo JUCE per la visualizzazione, navigazione e manipolazione di documenti PDF all'interno dell'ecosistema Ayra. Utilizzato nei plugin VST Ayra per mostrare documentazione tecnica (rider, schemi fixture, manuali GDTF) e nei pannelli informativi della DAW.

---

## Architettura

Il modulo è in migrazione dalla struttura v1 monolitica a un'architettura v2 separata per concern:

```
ayra_pdf/
├── ayra_pdf.h                         <- include aggregato del modulo
├── ayra_pdf.cpp                       <- include aggregato + piattaforma .mm
├── ayra_pdf.mm                        <- include Objective-C per macOS/iOS
│
├── engine/                            <- logica pura, headless-safe (Fase 2)
│   ├── ayra_PdfDocument.h/.cpp        <- API principale: open, render, search, extract
│   ├── ayra_PdfPage.h                 <- bounds, rotazione, metadata di pagina
│   └── ayra_PdfSearchResult.h        <- risultato di findText()
│
├── renderer/                          <- backend platform-specific (Fase 3)
│   ├── ayra_PdfRenderer.h             <- interfaccia pura (astrazione backend)
│   ├── mac/ayra_MacPdfRenderer.mm     <- CoreGraphics: macOS/iOS, zero dipendenze
│   └── pdfium/ayra_PdfiumRenderer.h/.cpp  <- PDFium: Windows/Linux/Android
│
├── widgets/                           <- JUCE Components (Fase 4)
│   ├── ayra_PdfViewComponent.h/.cpp   <- widget principale, sostituto di PDFComponent
│   └── look_and_feel/
│       ├── ayra_PdfLookAndFeelMethods.h
│       └── ayra_PdfDefaultLookAndFeel.h/.cpp
│
├── third_party/pdfium/                <- PDFium (Fase 3)
│   ├── include/                       <- headers (in git)
│   ├── mac/                           <- libpdfium.a fat universal (NON in git)
│   ├── win/                           <- pdfium.dll + pdfium.lib (NON in git)
│   ├── linux/                         <- libpdfium.a (NON in git)
│   ├── ios/                           <- libpdfium.a (NON in git)
│   └── android/                       <- libpdfium.so per ABI (NON in git)
│
├── scripts/
│   ├── setup_pdfium.sh                <- download binari macOS/Linux/iOS
│   └── setup_pdfium.ps1               <- download binari Windows
│
└── pdf_component/                     <- LEGACY v1 (backward compat, rimozione Fase 5)
    ├── PDFComponent.h/.cpp
    ├── Mac_PDF_core/
    ├── Windows_PDF_core/
    └── Linux_PDF_core/
```

### Separazione engine / renderer / widget

| Layer | Dipendenze | Headless | Descrizione |
|-------|-----------|---------|-------------|
| `engine/` | `juce_core` | ✓ | Stato documento, navigazione, ricerca, estrazione testo |
| `renderer/` | `juce_core` + CoreGraphics o PDFium | ✓ | Rasterizzazione pagine in `juce::Image` |
| `widgets/` | `juce_gui_basics` + engine + renderer | ✗ | Componenti visuali interattivi |

L'engine non include mai header GUI. Un agente server-side o un renderer offline possono usare solo `engine/` + `renderer/` senza aprire alcuna finestra.

---

## Platform Support Matrix

| Feature | macOS | iOS | Windows | Linux | Android |
|---------|-------|-----|---------|-------|---------|
| Rendering pagine | ✓ | ✓ | ✓* | ✓* | ✓* |
| Navigazione pagine | ✓ | ✓ | ✓* | ✓* | ✓* |
| Zoom + Pan | ✓ | ✓ | ✓* | ✓* | ✓* |
| Ricerca testo | ✓ | ✓ | ✓* | ✓* | ✓* |
| Estrazione testo | ✓ | ✓ | ✓* | ✓* | ✓* |
| Export PDF | ✓ | ✓ | ✓* | ✓* | ✓* |
| Headless (no GUI) | ✓ | ✓ | ✓* | ✓* | ✓* |
| Backend | CoreGraphics | CoreGraphics | PDFium | PDFium | PDFium |
| Setup extra | nessuno | nessuno | setup_pdfium | setup_pdfium | setup_pdfium |

`*` = richiede PDFium installato via `scripts/setup_pdfium.sh/.ps1` — implementazione in Fase 3.

---

## Quick Start

### PdfDocument — uso headless (engine puro)

```cpp
#include <ayra_pdf/ayra_pdf.h>

// --- Apertura da file ---
ayra::PdfDocument doc;
if (doc.open (juce::File ("/path/to/manual.pdf")))
{
    int pages = doc.getPageCount();     // numero totale pagine

    // Rendering a 2x per HiDPI
    juce::Image img = doc.renderPage (0, 2.0f);

    // Ricerca testo
    auto results = doc.findText ("velocita");
    for (const auto& r : results)
    {
        DBG ("pag " + juce::String (r.pageIndex) + " bounds: " + r.bounds.toString());
    }

    // Estrazione testo raw UTF-8
    juce::String text = doc.extractText (0);

    // Export
    doc.save (juce::File ("/path/to/output.pdf"));
}

// --- Apertura da memoria ---
juce::MemoryBlock block;
// ... popola block ...
doc.openFromMemoryBlock (block.getData(), (int) block.getSize());
```

### PdfViewComponent — widget interattivo

```cpp
#include <ayra_pdf/ayra_pdf.h>

class MyEditor : public juce::AudioProcessorEditor
{
public:
    MyEditor (MyProcessor& p) : AudioProcessorEditor (p)
    {
        addAndMakeVisible (pdfView);

        pdfView.loadDocument ("/path/to/manual.pdf");
        pdfView.setPageNumber (1);

        // Callback inline
        pdfView.onPageChanged = [](int page) { DBG ("pagina: " + juce::String (page)); };

        // Oppure Listener formale
        pdfView.addListener (this);

        setSize (800, 600);
    }

    void resized() override { pdfView.setBounds (getLocalBounds()); }

private:
    ayra::PdfViewComponent pdfView;
};
```

---

## API Reference

### `ayra::PdfDocument` (engine headless)

```cpp
class PdfDocument
{
public:
    // Apertura
    bool open (const juce::File& file) noexcept;
    bool openFromMemoryBlock (const void* data, int sizeInBytes) noexcept;
    void close() noexcept;

    // Stato
    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] int  getPageCount() const noexcept;

    // Rendering
    // scale: 1.0 = 72dpi nativo, 2.0 = 144dpi HiDPI
    [[nodiscard]] juce::Image renderPage (int pageIndex, float scale = 1.0f) const noexcept;

    // Navigazione / metadati
    [[nodiscard]] PdfPage getPage (int pageIndex) const noexcept;

    // Ricerca testo (message thread)
    [[nodiscard]] juce::Array<PdfSearchResult> findText (const juce::String& query) const;

    // Estrazione testo UTF-8 (message thread)
    [[nodiscard]] juce::String extractText (int pageIndex) const;

    // Serializzazione
    bool save (const juce::File& file) const noexcept;
    bool saveToMemoryBlock (juce::MemoryBlock& dest) const noexcept;
};
```

### `ayra::PdfPage`

```cpp
struct PdfPage
{
    int                     index    {};      // 0-based
    juce::Rectangle<float>  bounds   {};      // larghezza/altezza in punti PDF (1pt = 1/72")
    int                     rotation {};      // 0, 90, 180, 270 gradi

    [[nodiscard]] float getAspectRatio() const noexcept;
};
```

### `ayra::PdfSearchResult`

```cpp
struct PdfSearchResult
{
    int                    pageIndex {};      // pagina dove e' stato trovato (0-based)
    juce::String           text      {};      // testo corrispondente
    juce::Rectangle<float> bounds    {};      // bounding box in coordinate pagina
};
```

### `ayra::PdfViewComponent` (widget — Fase 4)

```cpp
class PdfViewComponent : public juce::Component
{
public:
    // Caricamento
    void loadDocument (const juce::String& filePath);
    void loadDocumentFromMemoryBlock (const void* data, int sizeInBytes);
    void getMemoryBlockFromDocument (juce::MemoryBlock& dest);
    void exportCurrentDocument (const juce::String& name, const juce::String& folderPath) const;

    // Stato documento
    [[nodiscard]] bool thereIsADocumentLoaded() const;
    [[nodiscard]] int  getTotPagesNum() const;
    [[nodiscard]] int  getCurrentPageOnScreen() const;

    // Navigazione
    void setPageNumber (int pageNumber);      // 1-based (display)

    // Viewport
    [[nodiscard]] float getCurrentPageZoom() const;
    void setCurrentPageZoom (float zoom, juce::Point<float> handlePoint);
    void setCurrentPageZoom (float zoom, juce::Point<int> handlePoint);

    [[nodiscard]] juce::Point<float>     getCurrentPageTopLeftPosition() const;
    void setCurrentPageTopLeftPosition (juce::Point<float> newPos);
    void setCurrentPageTopLeftPosition (juce::Point<int> newPos);

    [[nodiscard]] juce::Rectangle<float> getCurrentPageBounds() const;
    [[nodiscard]] float getDocumentWidth()  const;
    [[nodiscard]] float getDocumentHeight() const;

    // Listener formale
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void pdfPageChanged (PdfViewComponent*, int newPage) = 0;
        virtual void pdfDocumentLoaded (PdfViewComponent*) {}
        virtual void pdfDocumentClosed (PdfViewComponent*) {}
    };
    void addListener    (Listener*);
    void removeListener (Listener*);

    // Callback inline
    std::function<void (int)>  onPageChanged;
    std::function<void()>      onDocumentLoaded;
    std::function<void()>      onDocumentClosed;
};
```

---

## Backward Compatibility

La v1 esponeva `ayra::PDFComponent`. Tutta l'API e' identica in `PdfViewComponent`. A partire dalla Fase 5 sara' aggiunto l'alias:

```cpp
namespace ayra { using PDFComponent = PdfViewComponent; }
```

Fino alla Fase 5, `PDFComponent` rimane nella cartella `pdf_component/` con le stesse firme. I consumer esistenti non richiedono alcuna modifica.

**Firme identiche tra v1 e v2:**

| Metodo v1 (PDFComponent) | Metodo v2 (PdfViewComponent) |
|---|---|
| `loadDocument(juce::String)` | `loadDocument(const juce::String&)` |
| `setPageNumber(int)` | `setPageNumber(int)` |
| `getTotPagesNum()` | `getTotPagesNum()` |
| `getCurrentPageOnScreen()` | `getCurrentPageOnScreen()` |
| `setCurrentPageZoom(float, Point)` | `setCurrentPageZoom(float, Point)` |
| `exportCurrentDocument(name, path)` | `exportCurrentDocument(name, path)` |
| `loadDocumentFromMemoryBlock(void*, int)` | `loadDocumentFromMemoryBlock(void*, int)` |
| `getMemoryBlockFromDocument(MemoryBlock&)` | `getMemoryBlockFromDocument(MemoryBlock&)` |

**Aggiunte in v2 non presenti in v1:**
- `Listener` formale con `addListener`/`removeListener`
- Callback `std::function` (onPageChanged, onDocumentLoaded, onDocumentClosed)
- `LookAndFeel` customizzabile
- `PdfDocument` headless separato dal widget

---

## Build Configuration

### Projucer

**Per tutti i sistemi — primo passo obbligatorio:**
Aprire il progetto in Projucer → **Modules** → **+** → *Add a module from a specified folder* → selezionare `Juce Modules/ayra_pdf/`. La dipendenza `juce_gui_extra` viene aggiunta automaticamente.

**macOS / iOS** — nessun altro passo. CoreGraphics e' un framework di sistema, zero configurazione extra.

**Windows** (Fase 3 — PDFium):

| Campo Projucer | Valore |
|---|---|
| Extra Library Search Paths | `path\to\ayra_pdf\third_party\pdfium\win` |
| External Libraries to Link | `pdfium` |

Distribuire `pdfium.dll` nella stessa cartella del plugin (post-build step).

**Linux** (Fase 3 — PDFium):

| Campo Projucer | Valore |
|---|---|
| Extra Library Search Paths | `/path/to/ayra_pdf/third_party/pdfium/linux` |
| External Libraries to Link | `pdfium` |
| Extra Linker Flags | `-Wl,-rpath,$$ORIGIN` |

**Android** — configurare via CMakeLists.txt (vedi sezione CMake).

> **Nota distribuzione macOS (dylib):** le release PDFium >= chromium/7000 distribuiscono `.dylib`
> invece di `.a` su macOS. Se in futuro si abilita PDFium anche su Mac, il `.dylib` va bundled
> nel `.vst3` package (`Contents/MacOS/`) con flag `-Wl,-rpath,@loader_path`. Vedi `SETUP.md`
> sezione macOS per i dettagli.

### CMake

```cmake
# macOS / iOS: nessuna configurazione aggiuntiva

# Windows
if (WIN32)
    target_link_libraries (MyPlugin PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Juce Modules/ayra_pdf/third_party/pdfium/win/pdfium.lib")
    # Copiare pdfium.dll nella directory di output
    add_custom_command (TARGET MyPlugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Juce Modules/ayra_pdf/third_party/pdfium/win/pdfium.dll"
        "$<TARGET_FILE_DIR:MyPlugin>")
endif()

# Linux
if (UNIX AND NOT APPLE AND NOT ANDROID)
    target_link_libraries (MyPlugin PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Juce Modules/ayra_pdf/third_party/pdfium/linux/libpdfium.a")
    target_link_options (MyPlugin PRIVATE "-Wl,-rpath,\$ORIGIN")
endif()

# Android
if (ANDROID)
    target_link_libraries (MyPlugin PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Juce Modules/ayra_pdf/third_party/pdfium/android/${ANDROID_ABI}/libpdfium.so")
endif()
```

---

## PDFium

[PDFium](https://pdfium.googlesource.com/pdfium/) e' il motore PDF open source di Google, estratto da Chromium. I binari precompilati usati da questo modulo provengono dal progetto [pdfium-binaries](https://github.com/bblanchon/pdfium-binaries) di Benoit Blanchon.

**Licenza:** Apache 2.0 — commercial-safe, nessun obbligo copyleft.

**Uso nei backend:**
- macOS / iOS: non usato. CoreGraphics nativo e' il backend.
- Windows / Linux / Android: PDFium e' il backend esclusivo.

**Dimensione binari (stima):**
- Static library (Linux/iOS): ~15-20 MB aggiuntivi al plugin
- DLL dinamica (Windows): ~10-15 MB distribuita separatamente
- macOS: 0 MB (CoreGraphics)

**Dove si trovano i file:**

| Percorso | In git | Descrizione |
|----------|--------|-------------|
| `third_party/pdfium/include/` | ✓ | Headers C dell'API pubblica PDFium |
| `third_party/pdfium/mac/libpdfium.dylib` | ✗ | Fat binary universal (arm64+x86_64) — release >= chromium/7000 |
| `third_party/pdfium/win/pdfium.dll` | ✗ | DLL Windows x64 |
| `third_party/pdfium/win/pdfium.lib` | ✗ | Import library |
| `third_party/pdfium/linux/libpdfium.a` | ✗ | Static x86_64 |
| `third_party/pdfium/ios/libpdfium.a` | ✗ | Fat binary arm64+x86_64 sim |
| `third_party/pdfium/android/arm64-v8a/` | ✗ | .so per ABI arm64 |
| `third_party/pdfium/android/x86_64/` | ✗ | .so per ABI x86_64 |

**Download:** eseguire `scripts/setup_pdfium.sh` (macOS/Linux) o `scripts/setup_pdfium.ps1` (Windows) dopo aver clonato il repository. Vedi `SETUP.md` per i dettagli.

**Init PDFium:**
`FPDF_InitLibrary()` viene chiamato una sola volta per processo tramite `std::call_once` interno a `PdfiumRenderer`. Non e' necessario chiamarlo manualmente.

**Thread safety:**
PDFium non e' thread-safe a livello di documento. Ogni `PdfDocument` possiede il proprio `FPDF_DOCUMENT` e non condivide stato con altre istanze. Chiamate parallele su istanze distinte sono sicure.

**Aggiornamento versione:**
Modificare `PDFIUM_VERSION` in `scripts/setup_pdfium.sh` / `setup_pdfium.ps1`, poi rieseguire lo script. I binari vengono scaricati da GitHub Releases di `pdfium-binaries`.

---

## Headless Mode

Per build server-side, CI headless o renderer offline, definire:

```cpp
#define AYRA_PDF_HEADLESS
```

Prima di includere il modulo (o come define nel Projucer / CMakeLists.txt).

Con questa define:
- I widget in `widgets/` non vengono inclusi.
- La dipendenza da `juce_gui_basics` e `juce_gui_extra` non e' necessaria.
- Solo `juce_core` e il backend renderer sono richiesti.

Esempio uso headless (batch rendering):

```cpp
// Nessuna GUI richiesta
#define AYRA_PDF_HEADLESS
#include <ayra_pdf/ayra_pdf.h>

void renderPdfToPng (const juce::File& pdfFile, const juce::File& outDir)
{
    ayra::PdfDocument doc;
    if (!doc.open (pdfFile)) { return; }

    for (int i = 0; i < doc.getPageCount(); ++i)
    {
        juce::Image img = doc.renderPage (i, 2.0f);
        juce::File out = outDir.getChildFile ("page_" + juce::String (i + 1) + ".png");
        juce::PNGImageFormat png;
        juce::FileOutputStream stream (out);
        png.writeImageToStream (img, stream);
    }
}
```

---

## Fasi di Sviluppo

| Fase | Stato | Contenuto |
|------|-------|-----------|
| Fase 1 — Legacy v1 | ✓ completa | `pdf_component/PDFComponent` con backend macOS (CoreGraphics) funzionante, stub Windows/Linux |
| Fase 2 — Engine headless | in progress | `engine/PdfDocument`, `PdfPage`, `PdfSearchResult` — separazione logica da GUI |
| Fase 3 — Renderer layer | in progress | `renderer/PdfRenderer` interfaccia pura + `MacPdfRenderer` (CoreGraphics) + `PdfiumRenderer` (PDFium) |
| Fase 4 — Widget v2 | pianificata | `widgets/PdfViewComponent` — sostituto di `PDFComponent` con LookAndFeel, Listener, std::function |
| Fase 5 — Alias e cleanup | pianificata | `using PDFComponent = PdfViewComponent`, rimozione `pdf_component/`, aggiornamento dipendenze Projucer |

**Stato attuale (Fase 1):**
- macOS / iOS: rendering funzionante via CoreGraphics (`MacPDFViewComponent` wrappa `PDFView` Objective-C)
- Windows: stub con parse minimale PDF + rendering placeholder (nessun backend reale)
- Linux: stub con chiamata a `pdftocairo` (dipendenza di sistema, non inclusa)
- Android: non implementato

**Issues noti nella v1 (vedi commenti in `PDFComponent.h`):**
- macOS: gestione zoom dal punto di handle non completamente corretta
- macOS: gesture multi-touch non implementate
- macOS: scroll quando il documento e' piu' grande della view non implementato
- macOS: inspector/ricerca pagina non implementato
- Windows / Linux / Android: tutto da implementare con PDFium (Fase 3)

---

## Licenza

Copyright Ayra Soft. Tutti i diritti riservati.

PDFium (usato su Windows/Linux/Android): Apache License 2.0.
CoreGraphics (usato su macOS/iOS): framework di sistema Apple, nessuna licenza aggiuntiva.
