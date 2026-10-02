# Architettura — ayra_pdf

**Autorita' stato operativo:** `_docs/02_stato_attuale.md`.

## Layer

```text
ayra_pdf
├── engine/      PdfDocument + value objects
├── renderer/    PdfRenderer + backend Apple/PDFium
└── widgets/     PdfViewComponent + LookAndFeel (opzionale)
```

Non esiste un layer legacy parallelo.

## Engine

`PdfDocument` e' una factory/facade. Possiede un solo `std::unique_ptr<PdfRenderer>` e
delega 1:1:
- Apple -> `MacPdfRenderer`;
- Windows/Linux/Android -> `PdfiumRenderer`.

Il facade non possiede parsing, raster, safety limits o search algorithms.

## Coordinate canoniche

`PdfPage.bounds` e' il visible page box canonico (MediaBox intersecato con CropBox). `PdfSearchResult.bounds` resta in PDF page/user space:
- 1 punto = 1/72 pollice;
- origine lower-left;
- asse Y verso l'alto.

`PdfPage::getDisplaySize()` e' l'owner canonico della dimensione dopo /Rotate.

La conversione page-space -> widget-space appartiene esclusivamente a
`PdfViewComponent` per l'overlay visuale.

## Backend Apple

`MacPdfRenderer`:
- CoreGraphics: load, page metadata, raster;
- PDFKit: estrazione testo e ricerca;
- save: snapshot byte-preserving del provider;
- lock per istanza;
- cache PDFKit derivata dagli stessi byte del documento, mai seconda source of truth.

## Backend PDFium

`PdfiumRenderer`:
- lifecycle library 0->1 / 1->0;
- una lock process-wide per tutte le API FPDF;
- load file e memory convergono su owned memory + `FPDF_LoadMemDocument64`;
- save byte-preserving dai backing bytes;
- raster diretto nel buffer JUCE;
- text/search via `fpdf_text.h`.

## Safety limits

L'owner unico e' `renderer/ayra_PdfSafetyLimits.h`.

I limiti proteggono document bytes, raster, testo, query e numero di risultati. Sono resource
guard del modulo, non limiti del formato PDF.

## Widget

`PdfViewComponent` e' view state, non document owner semantico:
- mantiene `std::shared_ptr<PdfDocument>` per lifetime asincrono;
- pagina UI 1-based, engine 0-based;
- cache immagine derivata;
- render/search su worker condiviso;
- richieste coalesced, latest-request-wins;
- publication sul Message Thread con generation token;
- `paint()` fa solo compositing;
- `setDocument(nullptr)` e' il clear canonico del documento visualizzato;
- zoom/pan/HiDPI/search overlay sono stato visuale.

Il documento condiviso non va mutato concorrentemente dal caller.

## LookAndFeel

Ogni elemento disegnabile ha hook dedicato:
- background;
- no-document;
- page background;
- page shadow;
- search highlight.

Il fallback default e' stateless.

## Headless

`AYRA_PDF_HEADLESS` esclude completamente `widgets/`.

Dipendenza obbligatoria: `juce_graphics`.
`juce_gui_basics` e' richiesta solo dal target consumer che usa la GUI.
`juce_gui_extra` non e' una dipendenza.

## Compatibilita' del widget

Le implementazioni platform-specific v1 sono state eliminate. `PdfViewComponent` e' l'unica
implementazione canonica del widget.

Il nome pubblico `PDFComponent` resta disponibile come alias source-compatible di
`PdfViewComponent` per preservare i consumer esistenti. L'alias e' lo stesso tipo: non introduce
un layer legacy, un adapter, un backend alternativo o una seconda source of truth.
