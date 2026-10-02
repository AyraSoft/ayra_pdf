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
│   ├── mac/ayra_MacPdfRenderer.mm     <- CoreGraphics + PDFKit: macOS/iOS
│   └── pdfium/ayra_PdfiumRenderer.h/.cpp  <- PDFium: Windows/Linux/Android
│
├── widgets/                           <- JUCE Components (Fase 4)
│   ├── ayra_PdfViewComponent.h/.cpp   <- widget principale, sostituto di PDFComponent
│   └── look_and_feel/
│       ├── ayra_PdfLookAndFeelMethods.h
│       └── ayra_PdfDefaultLookAndFeel.h    <- implementazione inline nell'header (nessun .cpp)
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
| `engine/` | core + tipi grafici usati dall'API (`juce::Image`) | senza finestre | Stato documento, navigazione, ricerca, estrazione testo |
| `renderer/` | tipi grafici JUCE + CoreGraphics/PDFKit o PDFium | senza finestre | Rasterizzazione + content model PDF |
| `widgets/` | JUCE GUI + engine + renderer | no | Componenti visuali interattivi |

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
| Backend target | CoreGraphics + PDFKit | CoreGraphics + PDFKit | PDFium | PDFium | PDFium |
| Setup extra | nessuno | nessuno | setup_pdfium | setup_pdfium | setup_pdfium |

`*` = richiede PDFium installato via `scripts/setup_pdfium.sh/.ps1` — implementazione in Fase 3.

> **Stato reale (2 ottobre 2026):** la matrice sopra descrive il target v2, non una
> certificazione runtime. `MacPdfRenderer` M01-M04 e' implementato e static-audited
> (CoreGraphics + PDFKit) e `PdfDocument` e' ora collegato al renderer tramite factory/delega.
> `PdfiumRenderer` e `PdfViewComponent` restano incompleti. Il solo path legacy
> attualmente operativo e' **macOS/AppKit**; il legacy usa `NSView` e non costituisce
> un'implementazione iOS. Build/test/CI restano external pending.

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
doc.open (block.getData(), block.getSize());
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
    [[nodiscard]] bool open (const juce::File& file) noexcept;
    [[nodiscard]] bool open (const void* data, size_t sizeBytes) noexcept;
    void close() noexcept;

    // Stato
    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] int  getPageCount() const noexcept;

    // Rendering (non-const: puo' aggiornare lo stato interno del renderer)
    // scale: 1.0 = 72dpi nativo, 2.0 = 144dpi HiDPI
    [[nodiscard]] juce::Image renderPage (int pageIndex, float scale = 1.0f) noexcept;

    // Navigazione / metadati
    [[nodiscard]] PdfPage getPage (int pageIndex) const noexcept;

    // Ricerca testo (message thread) — pageIndex = -1 cerca in tutto il documento
    [[nodiscard]] juce::Array<PdfSearchResult> findText (const juce::String& query, int pageIndex = -1) noexcept;

    // Estrazione testo UTF-8 (message thread)
    [[nodiscard]] juce::String extractText (int pageIndex) noexcept;

    // Serializzazione
    [[nodiscard]] bool save (const juce::File& file) const noexcept;
    [[nodiscard]] bool saveToMemoryBlock (juce::MemoryBlock& dest) const noexcept;
};
```

### `ayra::PdfPage`

```cpp
struct PdfPage
{
    int                     index    {};      // 0-based
    juce::Rectangle<float>  bounds   {};      // larghezza/altezza in punti PDF (1pt = 1/72")
    int                     rotation {};      // 0, 90, 180, 270 gradi

    // true se index >= 0 e bounds non vuote
    [[nodiscard]] bool isValid() const noexcept;
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

## Current-only contract

Il contratto corrente segue `AYRA_AI_ENGINEERING_MASTER.md`: **nessuna retrocompatibilita'
Ayra preventiva**. Il vecchio `PDFComponent` e' presente solo perche' oggi e' il path
funzionante mentre la v2 e' incompleta; non e' un'API da preservare.

Al cutover:
- `PdfViewComponent` sara' l'unico widget pubblico corrente;
- `pdf_component/` verra' rimosso;
- NON verra' introdotto `using PDFComponent = PdfViewComponent`;
- non resteranno adapter, fallback o doppio percorso legacy/v2;
- l'audit dei call-site AyraSoft verra' ripetuto nello stesso cambiamento.

Lo stato e il piano canonico sono in `_docs/02_stato_attuale.md`.

---

## Build Configuration

### Projucer

**Per tutti i sistemi — primo passo obbligatorio:**
Aprire il progetto in Projucer → **Modules** → **+** → *Add a module from a specified folder* → selezionare `Juce Modules/ayra_pdf/`. La dipendenza `juce_gui_extra` viene aggiunta automaticamente.

**macOS / iOS** — nessun setup third-party. CoreGraphics e PDFKit sono framework Apple di sistema e vengono dichiarati dal metadata del modulo.

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
- macOS / iOS: non usato dal backend corrente; il backend usa CoreGraphics + PDFKit di sistema.
- Windows / Linux / Android: PDFium e' il backend esclusivo.

**Dimensione binari (stima):**
- Static library (Linux/iOS): ~15-20 MB aggiuntivi al plugin
- DLL dinamica (Windows): ~10-15 MB distribuita separatamente
- macOS/iOS: 0 MB third-party per il backend corrente (CoreGraphics + PDFKit di sistema)

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

**Init PDFium (design Fase 3 — non ancora implementato):**
`PdfiumRenderer` e' attualmente uno skeleton: tutti i metodi sono `jassertfalse` / `return {}`. Il design previsto per la Fase 3 e' chiamare `FPDF_InitLibrary()` una sola volta per processo (con ref-count) e `FPDF_DestroyLibrary()` quando il count torna a zero — vedi i `TODO: Fase 3` in `renderer/pdfium/ayra_PdfiumRenderer.cpp`. Nessuna init e' eseguita oggi.

**Thread safety (design Fase 3):**
PDFium non e' thread-safe a livello di documento. Il design previsto e' che ogni `PdfDocument` possieda il proprio `FPDF_DOCUMENT` senza condividere stato con altre istanze, cosi' che chiamate parallele su istanze distinte siano sicure. Non ancora implementato (Fase 3).

**Aggiornamento versione:**
Modificare `PDFIUM_VERSION` in `scripts/setup_pdfium.sh` / `setup_pdfium.ps1`, poi rieseguire lo script. I binari vengono scaricati da GitHub Releases di `pdfium-binaries`.

---

## Headless Mode

`AYRA_PDF_HEADLESS` **oggi significa soltanto "escludi i widget dal codice del modulo"**.
Non significa ancora "nessuna dipendenza JUCE GUI": `ayra_pdf.h` include `juce_gui_extra`
e la metadata del modulo dichiara quella dipendenza.

Il codice puo' quindi essere usato senza aprire finestre, ma il vecchio claim dependency-minimal
headless non e' ancora realizzato. La risoluzione e' il microstep **H01** nel documento
canonico `_docs/02_stato_attuale.md`.

---

## Fasi di Sviluppo

La numerazione storica "Fase 1...7" resta nei documenti tecnici come provenance, ma non
e' piu' la source of truth dell'avanzamento. Il piano operativo corrente usa microstep
(`S00`, `M01`...`V01`) ed e' mantenuto esclusivamente in `_docs/02_stato_attuale.md`.

Sintesi al 2026-10-02:
- renderer Apple v2 M01-M04: **done (static)**;
- `PdfDocument`: D02 **done (static)** su tutti i backend dichiarati;
- renderer PDFium: P01-P03 **done (static)**;
- `PdfViewComponent`: parziale;
- legacy operativo: macOS/AppKit soltanto, destinato a rimozione current-only;
- iOS v2: codice backend presente ma integrazione end-to-end da verificare esternamente;
- build/test/CI: non eseguibili nell'ambiente AI corrente; verification esterna richiesta.

---

## Licenza

Copyright Ayra Soft. Tutti i diritti riservati.

PDFium (usato su Windows/Linux/Android): Apache License 2.0.
CoreGraphics e PDFKit (backend Apple): framework di sistema Apple, nessuna dipendenza third-party.
