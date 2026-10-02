# ayra_pdf

Modulo JUCE per lettura, rendering, ricerca, estrazione testo e visualizzazione PDF.

**Stato al 2026-10-02:** l'implementazione v2 e' completa a livello statico. In questo ambiente
non sono disponibili build, test binari o CI: la certificazione runtime cross-platform resta
esplicitamente **external pending**. Lo stato operativo canonico vive in
`_docs/02_stato_attuale.md`.

## Architettura

```text
ayra_pdf/
├── ayra_pdf.h / ayra_pdf.cpp / ayra_pdf.mm
├── engine/
│   ├── ayra_PdfDocument.h/.cpp
│   ├── ayra_PdfPage.h
│   └── ayra_PdfSearchResult.h
├── renderer/
│   ├── ayra_PdfRenderer.h
│   ├── ayra_PdfSafetyLimits.h
│   ├── mac/ayra_MacPdfRenderer.h/.mm
│   └── pdfium/ayra_PdfiumRenderer.h/.cpp
├── widgets/
│   ├── ayra_PdfViewComponent.h/.cpp
│   └── look_and_feel/
├── third_party/pdfium/
└── scripts/
```

Non esiste piu' un percorso legacy parallelo. `PdfDocument` e `PdfViewComponent` sono le API
canoniche. Il nome pubblico `PDFComponent` resta disponibile come alias source-compatible di
`PdfViewComponent` per i consumer esistenti: non seleziona codice o backend legacy.

### Backend

| Target | Backend | Stato codice |
|---|---|---|
| macOS | CoreGraphics + PDFKit | complete static |
| iOS | CoreGraphics + PDFKit | complete static |
| Windows | PDFium | complete static |
| Linux | PDFium | complete static |
| Android | PDFium | complete static |

"Complete static" non equivale a runtime verified: vedere la matrice V01 nel documento di stato.

## Dipendenze JUCE

Il core dipende soltanto da `juce_graphics` per `juce::Image` e i tipi geometrici.

Per usare il widget:
- aggiungere `juce_gui_basics` al target consumer;
- non definire `AYRA_PDF_HEADLESS`.

Per uso headless:
- definire `AYRA_PDF_HEADLESS=1`;
- `juce_gui_basics` e `juce_gui_extra` non sono richiesti dal modulo.

`juce_gui_extra` non e' una dipendenza di ayra_pdf.

## Quick start - engine

```cpp
#include <ayra_pdf/ayra_pdf.h>

ayra::PdfDocument document;

if (document.open (juce::File ("/path/manual.pdf")))
{
    const int pageCount = document.getPageCount();
    const auto page = document.getPage (0);
    const auto image = document.renderPage (0, 2.0f);
    const auto text = document.extractText (0);
    const auto matches = document.findText ("channel", 0);

    juce::MemoryBlock snapshot;
    (void) document.saveToMemoryBlock (snapshot);
}
```

Le API engine usano indici pagina **0-based**. `PdfPage.bounds` rappresenta il visible box (MediaBox intersect CropBox). Le bounds di `PdfPage` e
`PdfSearchResult` sono in PDF page space: 72 dpi, origine lower-left, Y-up.

## Quick start - widget

```cpp
class Viewer : public juce::Component
{
public:
    Viewer()
    {
        addAndMakeVisible (pdf);
        pdf.loadDocument ("/path/manual.pdf");
        pdf.setPageNumber (1); // UI 1-based
        pdf.setSearchQuery ("channel");
    }

    void resized() override
    {
        pdf.setBounds (getLocalBounds());
    }

private:
    ayra::PdfViewComponent pdf;
};
```

Il widget supporta:
- navigazione 1-based;
- raster asincrono e coalesced;
- zoom 0.1x..10x con anchor;
- pan, wheel e pinch;
- raster HiDPI separato dalle dimensioni logiche;
- overlay search della pagina corrente;
- LookAndFeel personalizzabile;
- Listener + `std::function`.

Per condividere un documento tra consumer usare
`setDocument(std::shared_ptr<PdfDocument>)`. Il documento condiviso non va mutato
concorrentemente durante l'uso.

## PDFium

Header e binari PDFium sono accoppiati dal manifest pinned `third_party/pdfium/pdfium_manifest.json`. Gli installer in `scripts/` verificano SHA-256 prima dell'estrazione.

PDFium non e' thread-safe: il backend serializza internamente tutte le chiamate FPDF con una
lock process-wide. Il caller deve comunque rispettare il lifetime dell'oggetto renderer/documento.

## Safety contract

- PDF esterni sono input non affidabili.
- Dimensione documento, raster, testo, query e numero risultati sono bounded.
- Il budget sorgente canonico e' esposto da `PdfDocument::getMaximumDocumentBytes()`.
- Open file/memory e' replacement transazionale: un candidato fallito preserva il documento attivo.
- Save file/memory e' byte-preserving e transazionale.
- Rendering/parsing/search non sono realtime-safe e non vanno chiamati dal processBlock.
- Il widget non esegue parsing o raster dentro `paint()`.

## Compatibilita' API senza percorso legacy

Il vecchio codice platform-specific di `PDFComponent` e' stato rimosso. Il nome pubblico
`PDFComponent` e' mantenuto come alias di `PdfViewComponent` per preservare i call-site e il
modo di istanziare il widget gia' usati dai consumer Ayra. Alias e nome canonico sono lo stesso
tipo e usano un solo percorso di implementazione; non esistono backend, adapter o fallback v1.

## Documentazione

- `_docs/01_architettura.md` - architettura corrente.
- `_docs/02_stato_attuale.md` - stato/roadmap canonici.
- `_docs/03_fase2_MacPdfRenderer.md` - dettaglio backend Apple.
- `_docs/04_fase3_PdfiumRenderer.md` - dettaglio backend PDFium.
- `_docs/05_fase4_PdfDocument.md` - facade engine.
- `_docs/06_fase5_PdfViewComponent.md` - widget.
- `_docs/07_deployment.md` - deployment.
- `_docs/08_verification_v01.md` - runbook V01.
- `_docs/00_storico.md` - provenance storica, non normativa.
