# PdfDocument — Engine Facade

**Stato operativo**: vedere `_docs/02_stato_attuale.md`.

## Contratto

`PdfDocument` e' esclusivamente factory + facade.

Factory compile-time:
- macOS/iOS -> `MacPdfRenderer`;
- Windows/Linux/Android -> `PdfiumRenderer`;
- altri target -> errore compile-time.

Ogni API pubblica esegue solo guardia difensiva e delega 1:1:
- open file/memory;
- save file/memory;
- close/state;
- page count/metadata;
- render;
- search;
- extract text.

Il facade non possiede safety limits, parser, cache pagina o policy platform-specific.

## Stato

D01 e D02 sono **done (static)**. Entrambi i renderer concreti sono implementati.
Non esiste un fallback legacy.

## Verification

Da eseguire esternamente:
- smoke `open -> getPage -> render -> findText -> extractText -> save`;
- tutti i target dichiarati;
- build headless con `AYRA_PDF_HEADLESS=1`.
