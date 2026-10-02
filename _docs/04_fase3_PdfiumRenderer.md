# PdfiumRenderer — Backend Windows/Linux/Android

**Stato operativo**: vedere `_docs/02_stato_attuale.md`.

## Scope

`PdfiumRenderer` e' il backend current-only per Windows, Linux e Android.

Non esiste fallback inert se PDFium manca: header/binario/link devono essere disponibili per
il target dichiarato.

## Threading process-wide

Gli header PDFium vendorizzati dichiarano che nessuna API FPDF e' thread-safe.

Owner canonico: un unico `juce::CriticalSection` process-wide.

Tutte le chiamate FPDF, anche su documenti differenti, sono serializzate dalla stessa lock.

Lifecycle:
- renderer count 0 -> 1: `FPDF_InitLibraryWithConfig`;
- renderer count 1 -> 0: `FPDF_DestroyLibrary`;
- nessun `once_flag` incompatibile con re-init.

## Load

File e memory convergono sullo stesso path owned-memory.

### File

- helper byte-source canonico del modulo;
- size bounded da `detail::maxDocumentBytes`;
- lettura chunked fuori dalla lock PDFium;
- snapshot `shared_ptr<const MemoryBlock>` owned e immutabile.

### Memory

- copia owned;
- `FPDF_LoadMemDocument64`;
- il backing resta vivo fino a `FPDF_CloseDocument`;
- commit transazionale.

## Metadata

`getPage()` usa:
- `FPDF_GetPageBoundingBox` = MediaBox intersect CropBox;
- `FPDFPage_GetRotation` = 0/1/2/3 -> 0/90/180/270.

Le bounds restano in PDF page space 72 dpi lower-left/Y-up.

## Raster

`renderPage()`:
- `FPDF_GetPageWidthF/HeightF` per la display size;
- /Rotate e' gia' riflessa dalla page geometry;
- `FPDF_RenderPageBitmap(... rotate = 0 ...)`;
- page content only, senza `FPDF_ANNOT`;
- backdrop bianco;
- `FPDF_RENDER_LIMITEDIMAGECACHE`;
- stessi raster safety limits del backend Apple.

Pixel layout:
- Windows/Linux little-endian: PDFium BGRA -> JUCE PixelARGB nativo;
- Android: `FPDF_REVERSE_BYTE_ORDER` per il layout RGBA JUCE.

Nessun post-process B/R.

## Save

Il renderer e' read-only e possiede sempre i byte sorgente.

Save:
- cattura di uno `shared_ptr` ai backing bytes sotto lock;
- file I/O/copia e reclamation del backing ritirato fuori dalla lock process-wide;
- `saveToFile` usa `replaceWithData`;
- `saveToMemory` pubblica solo dopo copia riuscita;
- output byte-preserving.

## Unicode extraction

`FPDFText_CountChars` e' bounded.

`FPDFText_GetText` produce code-unit UTF-16; il backend:
- usa buffer `FPDF_WCHAR`;
- converte code-unit per code-unit verso il tipo UTF-16 JUCE;
- non usa reinterpret-cast aliasing tra `unsigned short` e `wchar_t`;
- gestisce endianness esplicitamente;
- limita anche l'output UTF-8.

## Search

`FPDFText_FindStart(... flags = 0 ...)` -> case-insensitive.

Per ogni match:
- index/count validati;
- exact match text riletto via `FPDFText_GetText`;
- geometria via `FPDFText_CountRects/GetRect`;
- rect multiline uniti in un unico bounds;
- bounds finite/non-degenerate;
- max risultati bounded;
- overflow -> failure atomico.

Page/text/search handles hanno teardown RAII mentre la lock process-wide e' ancora detenuta.

## Verification esterna

V01 deve coprire:
- link/runtime PDFium per ogni target;
- multi-instance stress;
- MediaBox/CropBox differenti;
- /Rotate 0/90/180/270;
- Windows/Linux BGRA correctness;
- Android reverse byte order;
- Unicode BMP + surrogate pair;
- Type0/CMap;
- multiline search;
- save byte equality;
- malformed/truncated PDF.


## Source identity

`sourceBytesEqualFile()` cattura lo snapshot immutabile sotto lock PDFium, rilascia subito la lock
globale e confronta il file byte-per-byte tramite il helper comune. Nessun hash/fingerprint viene
usato come prova di identita'.
