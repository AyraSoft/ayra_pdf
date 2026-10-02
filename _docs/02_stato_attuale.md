# Stato Attuale — ayra_pdf

**Autore**: Ayra Soft  
**Aggiornato**: 2026-10-02

> Questo file e' la **Single Source of Truth** per stato, priorita' e verification del modulo.
> Le specifiche di fase descrivono i componenti; `00_storico.md` e' solo provenance.

## Regole operative

- Normativa: `AyraSoft/ayra_multiverse/_doc/AYRA_AI_ENGINEERING_MASTER.md`.
- Single implementation: nessun backend/adapter v1; il nome pubblico `PDFComponent` resta alias di `PdfViewComponent` per i consumer esistenti.
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
- render/search off Message Thread; publication via cancellable `AsyncUpdater`;
- richieste coalesced;
- cache raster con generation token, separata dai metadata sincroni della pagina corrente;
- navigazione UI 1-based con reset zoom 1.0 sul cambio pagina;
- zoom, pan, wheel, pinch;
- HiDPI con raster scale capped indipendentemente dallo zoom visuale ai budget canonici;
- overlay search ruotato correttamente;
- LookAndFeel e doppia API eventi, incluso document loaded/closed.

### Headless

Il modulo dichiara `juce_graphics` come unica dipendenza JUCE obbligatoria.
`AYRA_PDF_HEADLESS` esclude widget e ogni dipendenza GUI del modulo.

### Cutover v1 e compatibilita' API

Il percorso v1 e' stato eliminato, ma il successivo compile del consumer
`AyraSoft/ayra_audio_processors/ayra_internal_plugins/pdf_viewer/ayra_PdfViewer.h` ha mostrato
che l'audit org-wide del 2026-10-02 era incompleto: quel consumer istanzia ancora
`PDFComponent pdfComponent{}`.

Il contratto corretto e':
- nessun `pdf_component/`;
- nessun include legacy;
- `PDFComponent` resta alias pubblico di `PdfViewComponent`;
- nessun fallback platform-specific v1.

L'alias preserva il call-site e il modo di istanziare il widget senza riattivare il vecchio
percorso: `PDFComponent` e `PdfViewComponent` sono lo stesso tipo. Sono preservati anche i
sentinel pubblici usati dai consumer: senza documento, page count, current page e zoom
restituiscono `-1`.

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
- TSan sul protocollo worker/widget e PDFium, se compatibile; teardown widget/plugin con job in-flight.

## V01 preparation

Aggiunto `tests/ayra_PdfCoreTests.cpp`, incluso dal root solo con `JUCE_UNIT_TESTS`.
Durante il preflight statico sono stati inoltre chiusi:
- compatibilita' source-level `PDFComponent` protetta da compile guards e sentinel test;
- metadata pagina corrente indipendenti dal completamento del raster asincrono;
- ownership PDFKit compatibile ARC/non-ARC;
- visible page box uniforme tra Apple/PDFium (MediaBox intersect CropBox);
- geometry post-rotation centralizzata in `PdfPage::getDisplayBounds()`;
- publication worker GUI tramite `AsyncUpdater` cancellabile;
- self-lifetime del RenderState durante callback che possono distruggere il widget.
Il test genera in memoria una fixture PDF ASCII con:
- 4 pagine /Rotate 0/90/180/270;
- MediaBox e CropBox differenti;
- testo Type1 standard;
- xref/trailer reali.

Copre geometry pura, visible box, open/render/search/extract, save byte-preserving,
load transazionale, budget oversize, destination-on-failure, chiamate concorrenti sulla
stessa istanza e (su target PDFium) lavoro concorrente multi-instance sotto lock process-wide. Lo stress puro init/destroy 0->1->0 resta nel runbook V01 esterno.
Non e' stato eseguito in questa sessione.

## PDFium provisioning

Provisioning Windows/Linux/Android ora usa `third_party/pdfium/pdfium_manifest.json` come SSoT: `chromium/7857`, asset dinamici per architettura e SHA-256 ufficiali. Gli installer non usano piu' `latest` e non contengono percorsi PDFium Apple obsoleti.

Runbook: `_docs/08_verification_v01.md`.

## Completion gate

Il codice e' **implementation-complete staticamente** e V01 ha ora uno smoke test
cross-backend riproducibile. Le specifiche backend 03/04 sono state riallineate al codice corrente. La release non va dichiarata production-verified finche'
V01 non viene eseguito in un ambiente con build/test/CI.
