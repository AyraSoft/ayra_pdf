# Setup — ayra_pdf

## 1. Dipendenze JUCE

### Core / headless

Il modulo dichiara:

```text
dependencies: juce_graphics
```

Per una build senza GUI definire:

```text
AYRA_PDF_HEADLESS=1
```

In questa configurazione `ayra_pdf` non include widget e non richiede
`juce_gui_basics` o `juce_gui_extra`.

### GUI

Per usare `PdfViewComponent`, il target consumer deve aggiungere anche
`juce_gui_basics`. Se manca, il root header emette un errore esplicito.

`juce_gui_extra` non e' richiesto.

## 2. Apple — macOS / iOS

Backend: CoreGraphics + PDFKit, entrambi framework di sistema.

Il module metadata dichiara:
- macOS: `CoreGraphics PDFKit`;
- iOS: `CoreGraphics PDFKit`.

Non serve PDFium sui target Apple correnti.

## 3. Windows / Linux / Android

Backend: PDFium.

Gli header sono in:

```text
third_party/pdfium/include/
```

I binari di piattaforma vengono preparati con:
- Windows: `scripts/setup_pdfium.ps1`;
- Linux/Android: `scripts/setup_pdfium.sh`.

Il backend e' current-only: se PDFium e' dichiarato per il target ma binario/link mancano,
il build deve fallire; non esiste un backend inert di fallback.

## 4. Contratto runtime

- Non chiamare load/render/search/save dal thread audio realtime.
- Un `PdfDocument` condiviso con il widget tramite `std::shared_ptr` non va mutato
  concorrentemente.
- Su PDFium tutte le chiamate FPDF sono serializzate internamente process-wide.
- Su Apple le chiamate di una singola istanza sono serializzate internamente.

## 5. Verification ancora richiesta

In questo ambiente non vengono eseguiti build o CI. Prima della release eseguire V01:
macOS, iOS, Windows, Linux e Android, incluse fixture PDF malformate, Unicode, rotazioni,
HiDPI, gesture e stress di concorrenza PDFium.
