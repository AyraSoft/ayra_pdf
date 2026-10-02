# MacPdfRenderer — Backend Apple

**Stato operativo**: vedere `_docs/02_stato_attuale.md`.

## Scope

`MacPdfRenderer` e' il backend current-only per macOS e iOS.

Ownership:
- CoreGraphics: documento, provider, metadata e raster;
- PDFKit: content model testuale derivato;
- `renderer/ayra_PdfSafetyLimits.h`: limiti di risorse condivisi;
- `PdfPage`: visible box e display geometry canonica.

Non possiede viewport, zoom, pan o stato widget.

## Lifecycle e threading

Ogni istanza contiene una `juce::CriticalSection` re-entrant che serializza tutti gli
entry point.

Il caller deve comunque garantire che l'istanza non venga distrutta mentre una chiamata e'
attiva.

Teardown:
1. PDFKit cache;
2. `CGPDFDocumentRef`;
3. `CGDataProviderRef`;
4. backing `MemoryBlock`.

Le release native 1..3 restano serializzate dentro `apiLock`; l'ultima release del potenzialmente
grande snapshot C++ viene ritirata sotto lock e reclamata dopo lo sblocco.

Il boundary PDFKit supporta sia ARC sia non-ARC.

## Load

### File

- file esistente;
- size > 0 e <= `detail::maxDocumentBytes`;
- lettura bounded/chunked in snapshot byte owned immutabile;
- provider CoreGraphics creato sullo snapshot, non sul path mutabile;
- nuovo documento costruito prima del commit;
- failure lascia intatto il documento precedente;
- modifiche successive al file esterno non cambiano il documento gia' aperto.

### Memory

- null/zero/oversize rifiutati;
- copia owned dei byte;
- provider punta al backing owned;
- commit transazionale;
- `std::bad_alloc` -> failure.

## Page metadata

`PdfPage.bounds` usa il visible page box:

```text
visible box = MediaBox intersect CropBox
```

Coordinate:
- 72 dpi;
- origine lower-left;
- Y-up.

`rotation` e' normalizzata a 0/90/180/270.

`PdfPage::getDisplaySize()` e `getDisplayBounds()` sono gli owner della geometria derivata
post-rotation.

## Raster

`renderPage(pageIndex, scale)`:

- valida page/scale;
- usa display size post-rotation;
- max 16384 pixel per lato;
- max 64 Mi pixel;
- `juce::SoftwareImageType` ARGB;
- verifica pixel format, stride e buffer size;
- backdrop bianco;
- Quartz bitmap Y-up -> JUCE top-down;
- `CGPDFPageGetDrawingTransform(... kCGPDFCropBox ...)`;
- clip al visible box;
- `CGContextDrawPDFPage`.

La /Rotate viene gestita da CoreGraphics, non dal widget.

## Save

Il renderer e' read-only. Save file/memory copia lo snapshot sorgente immutabile:
- nessuna ricostruzione con `CGPDFContext`;
- metadata/outline/form/annotation structure non vengono appiattiti;
- destinazione memory transazionale;
- file via `replaceWithData`.

## Text extraction

PDFKit e' una cache derivata dallo stesso snapshot immutabile usato da CoreGraphics.
Il bridge usa un `CFData` no-copy sopra quei byte; il renderer mantiene esplicitamente il lifetime
di snapshot, CFData e PDFDocument.

`extractText(pageIndex)` usa `PDFPage.string`:
- page index validato;
- output Unicode;
- max UTF-8 bytes per pagina bounded;
- eccezioni Objective-C tradotte in failure.

Non esiste parser font/CMap proprietario.

## Search

`findText(query, pageIndex)`:
- query non vuota e bounded;
- case-insensitive;
- ricerca pagina per pagina;
- `PDFPage selectionForRange:`;
- `PDFSelection boundsForPage:`;
- match text reale;
- max risultati bounded;
- overflow -> failure atomico.

Le bounds restano in PDF page space. La conversione visuale appartiene a `PdfPage` + widget.

## Verification esterna

V01 deve coprire:
- ARC e non-ARC se entrambi supportati dal consumer;
- file/memory load;
- MediaBox/CropBox differenti;
- /Rotate 0/90/180/270;
- raster marker/color bars;
- Unicode/Type0/CMap;
- multiline search;
- save byte equality;
- malformed PDF;
- stress accesso concorrente sulla stessa istanza.
