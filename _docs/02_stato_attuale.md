# Stato Attuale — ayra_pdf

**Autore**: Ayra Soft  
**Aggiornato**: 2026-10-02

> Questo file e' la **Single Source of Truth** per stato, priorita' e verification del modulo.
> Le specifiche di fase descrivono i componenti; `00_storico.md` e' solo provenance.

## Regole operative

- Normativa: `AyraSoft/ayra_multiverse/_doc/AYRA_AI_ENGINEERING_MASTER.md`.
- Current-only: nessun alias, adapter o fallback per API Ayra precedenti.
- In questa sessione e' possibile leggere/scrivere codice, ma **non** eseguire build, test
  binari o CI.
- "done (static)" significa code/diff audit completato, non runtime verified.

## Stato sintetico

| ID | Deliverable | Stato |
|---|---|---|
| S00 | Governance, SSoT documentale, roadmap | **done (static)** |
| M01 | Apple lifecycle/load/metadata | **done (static)** |
| M02 | Apple raster bounded | **done (static)** |
| M03 | Apple save byte-preserving | **done (static)** |
| M04 | Apple PDFKit text/search | **done (static)** |
| D01 | PdfDocument facade Apple | **done (static)** |
| P01 | PDFium lifecycle/load/metadata | **done (static)** |
| P02 | PDFium save/raster | **done (static)** |
| P03 | PDFium text/search | **done (static)** |
| D02 | PdfDocument cross-platform | **done (static)** |
| W01 | Widget load/navigation/cache/notification | **done (static)** |
| W02 | Zoom/pan/HiDPI/gesture/search overlay | **done (static)** |
| H01 | Headless dependency envelope | **done (static)** |
| C01 | Rimozione completa percorso v1 | **done (static)** |
| V01 | Build/test/sanitizer cross-platform | **external pending** |

## Stato per layer

### Engine

`PdfDocument` e' una factory/facade reale senza stub. Tutta la semantica PDF vive nei
renderer concreti.

### Apple

`MacPdfRenderer` usa CoreGraphics + PDFKit:
- load file/memory bounded e transazionale;
- metadata page-space + /Rotate;
- raster ARGB bounded;
- save file/memory byte-preserving;
- text/search Unicode;
- lock per istanza.

### PDFium

`PdfiumRenderer`:
- lifecycle process-wide corretto;
- tutte le API FPDF serializzate globalmente;
- owned memory load;
- save byte-preserving;
- raster diretto e bounded;
- text/search UTF-16 e bounds reali;
- nessun fallback headers-missing.

### Widget

`PdfViewComponent`:
- ownership asincrona tramite `std::shared_ptr<PdfDocument>`;
- render/search off Message Thread;
- richieste coalesced;
- cache con generation token;
- navigazione UI 1-based;
- zoom, pan, wheel, pinch;
- HiDPI;
- overlay search ruotato correttamente;
- LookAndFeel e doppia API eventi.

### Headless

Il modulo dichiara `juce_graphics` come unica dipendenza JUCE obbligatoria.
`AYRA_PDF_HEADLESS` esclude widget e ogni dipendenza GUI del modulo.

### Current-only cutover

L'audit org-wide AyraSoft del 2026-10-02 non ha trovato consumer di `PDFComponent` fuori da
`ayra_pdf`. Il percorso v1 e' stato quindi eliminato nello stesso cutover:
- nessun `pdf_component/`;
- nessun include legacy;
- nessun alias `PDFComponent`;
- nessun fallback platform-specific v1.

L'interim rename `AyraLegacyPDFView`, introdotto per eliminare la collisione con
`PDFKit.PDFView`, non e' piu' codice di produzione perche' l'intero legacy e' stato rimosso.

## Invarianti correnti

1. PDF e payload esterni sono input non affidabili e bounded.
2. `PdfPage.bounds` = MediaBox intersect CropBox; page/search bounds usano PDF page space, 72 dpi, lower-left/Y-up.
3. Il widget e' l'unico owner della conversione page-space -> widget-space.
4. Save e' byte-preserving; nessun backend ridisegna un PDF non modificato per salvarlo.
5. PDFium ha una sola chiamata FPDF alla volta nell'intero processo.
6. Rendering/search/parsing non entrano nel thread audio realtime.
7. `paint()` non effettua parsing o raster.
8. Cache e content model derivati hanno invalidazione/lifetime espliciti.
9. Nessun percorso legacy parallelo e' ammesso.

## V01 — verification esterna richiesta

### macOS
- build Debug/Release;
- file/memory load;
- MediaBox non-zero + CropBox differente/intersecato;
- /Rotate 0/90/180/270;
- raster color bars + marker ai quattro angoli;
- PDFKit Unicode/Type0/CMap/search multiline;
- widget zoom/pan/pinch/HiDPI/search overlay;
- save byte equality.

### iOS
- build/link CoreGraphics + PDFKit senza AppKit;
- stessa suite engine;
- widget touch/pinch e scale device.

### Windows
- link/runtime PDFium;
- BGRA -> JUCE ARGB correctness;
- Unicode/search;
- save equality;
- widget HiDPI.

### Linux
- PDFium link/deployment;
- render/search/save;
- headless build.

### Android
- ABI PDFium;
- reverse-byte-order raster;
- touch/pinch;
- headless/core smoke.

### Safety
- malformed/truncated PDF;
- document/raster/text/query budgets;
- stress multi-instance PDFium;
- ASan/UBSan dove disponibili;
- TSan sul protocollo worker/widget e PDFium, se compatibile.

## Completion gate

Il codice e' **implementation-complete staticamente**. La release non va dichiarata
production-verified finche' V01 non viene eseguito in un ambiente con build/test/CI.
