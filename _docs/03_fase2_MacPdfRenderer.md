# Fase 2 — MacPdfRenderer (CoreGraphics + PDFKit)

**Autore**: Ayra Soft  
**Data creazione**: 2026-05-26  
**Stato operativo**: vedere `_docs/02_stato_attuale.md` (source of truth)

---

## Obiettivo

Implementare il backend Apple current-only. Il legacy viene usato solo come riferimento storico:
CoreGraphics possiede lifecycle/raster, PDFKit possiede text extraction/search.

Deliverable della Fase 2:
- `loadFromFile` — caricamento da path
- `loadFromMemory` — caricamento da buffer
- `saveToFile` — copia byte-preserving e transazionale dei dati sorgente
- `saveToMemory` — copia byte-preserving e transazionale dei dati sorgente
- `getPageCount` — conteggio pagine strutturale
- `getPage` — bounds in PDF user space + rotazione; la conversione viewport appartiene al widget
- `renderPage` — rasterizzazione in `juce::Image` via `CGBitmapContext`
- `extractText` — estrazione Unicode via PDFKit `PDFPage.string`
- `findText` — ricerca case-insensitive via PDFKit `PDFSelection`, bounds esatti in page space

---

## Prerequisiti

- Solo macOS e iOS — tutto il file e' sotto `#if JUCE_MAC || JUCE_IOS`
- Nessuna dipendenza third-party: CoreGraphics e PDFKit sono framework Apple di sistema
- Target documentati: macOS 11+ / iOS 14+; PDFKit e' disponibile su entrambi
- Il modulo deve dichiarare esplicitamente i framework Apple che usa

---

## Implementazione loadFromFile()

Migrazione da `MacPDFViewComponent::loadDocument()` in `MacPDFComponent.mm:63-74`.

```objc
bool MacPdfRenderer::loadFromFile (const juce::File& file) noexcept
{
    close(); // rilascia il documento precedente

    NSString* path = [NSString stringWithUTF8String: file.getFullPathName().toRawUTF8()];
    NSURL* pdfURL  = [NSURL fileURLWithPath: path];

    CGPDFDocumentRef doc = CGPDFDocumentCreateWithURL ((__bridge CFURLRef) pdfURL);
    if (doc == nullptr) { return false; }

    impl->document = doc;
    impl->pdfData.reset(); // caricamento da file: niente copia in memoria
    return true;
}
```

---

## Implementazione loadFromMemory()

Migrazione da `MacPDFViewComponent::loadDocumentFromMemoryBlock()` in `MacPDFComponent.mm:236-253`.

```objc
bool MacPdfRenderer::loadFromMemory (const void* data, size_t sizeBytes) noexcept
{
    close();

    // Copia i dati: CGDataProvider mantiene un puntatore interno al buffer,
    // quindi il buffer deve sopravvivere al documento. Lo salviamo in impl->pdfData.
    impl->pdfData.replaceAll (data, sizeBytes);

    NSData* nsData = [NSData dataWithBytesNoCopy: impl->pdfData.getData()
                                          length: impl->pdfData.getSize()
                                    freeWhenDone: NO]; // proprietario e' impl->pdfData
    CGDataProviderRef provider = CGDataProviderCreateWithCFData ((__bridge CFDataRef) nsData);
    CGPDFDocumentRef doc = CGPDFDocumentCreateWithProvider (provider);
    CGDataProviderRelease (provider);

    if (doc == nullptr) { impl->pdfData.reset(); return false; }
    impl->document = doc;
    return true;
}
```

---

## Save file / memory — implementazione corrente byte-preserving

Il renderer v2 e' read-only: salvare significa preservare esattamente il documento caricato,
non ridisegnare le pagine in un nuovo PDF. La ricostruzione via `CGPDFContext` e' stata scartata
perche' puo' perdere metadata, outline, annotation, form state e altre strutture non grafiche.

M03 usa `CGDataProviderCopyData` per ottenere uno snapshot bounded dei byte sorgente.

- `saveToFile`: `juce::File::replaceWithData`, quindi sostituzione tramite temporary file.
- `saveToMemory`: costruisce un nuovo `MemoryBlock` e lo pubblica solo dopo copia riuscita.
- failure: la destinazione precedente resta intatta.
- limite: `detail::maxDocumentBytes`, owner canonico in `renderer/ayra_PdfSafetyLimits.h`.

---

## Implementazione getPageCount()

```objc
int MacPdfRenderer::getPageCount() const noexcept
{
    if (impl->document == nullptr) { return 0; }
    return (int) CGPDFDocumentGetNumberOfPages (impl->document);
}
```

---

## Implementazione getPage()

```objc
PdfPage MacPdfRenderer::getPage (int pageIndex) const noexcept
{
    if (impl->document == nullptr) { return {}; }

    // CGPDFDocumentGetPage e' 1-based
    CGPDFPageRef page = CGPDFDocumentGetPage (impl->document, (size_t)(pageIndex + 1));
    if (page == nullptr) { return {}; }

    CGRect mediaBox = CGPDFPageGetBoxRect (page, kCGPDFCropBox);
    int rotationCG  = CGPDFPageGetRotationAngle (page); // 0, 90, 180, 270

    PdfPage result;
    result.index  = pageIndex;
    // Bounds canoniche in PDF user space (Y-up). Nessuna conversione viewport qui:
    // quella responsabilita' appartiene al consumer/widget.
    result.bounds   = { (float)mediaBox.origin.x, (float)mediaBox.origin.y,
                        (float)mediaBox.size.width, (float)mediaBox.size.height };
    result.rotation = rotationCG;
    return result;
}
```

---

## renderPage() — implementazione corrente

M02 rasterizza direttamente in un `juce::SoftwareImageType` ARGB.

Contratto:
- `scale` deve essere finita e > 0;
- dimensioni ruotate correttamente per 90/270 gradi;
- max 16384 px per lato e 64 Mi pixel totali (policy canonica condivisa);
- verifica `BitmapData`: ARGB, pixel stride, line stride e `bitmap.size`;
- CoreGraphics usa `kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host`,
  compatibile col layout native `juce::PixelARGB` su Apple;
- backdrop bianco deterministico;
- flip esplicito Quartz Y-up -> buffer JUCE top-down;
- `CGPDFPageGetDrawingTransform` gestisce visible box (MediaBox ∩ CropBox) e /Rotate;
- `std::bad_alloc` viene tradotto in immagine invalida.

La verifica visiva/runtime deve coprire una pagina asimmetrica con marker ai quattro angoli,
color bars, visible box (MediaBox ∩ CropBox) non-zero e rotazioni 0/90/180/270.

---

## Fix zoom anchor point

Il bug nel codice legacy (`MacPDFComponent.mm:160-163`) produce un anchor sempre in (0,0).

```cpp
// BUG v1 — anchor sempre in origine, indipendente dal vecchio zoom
float xOffset = handlePoint.getX() - (handlePoint.getX() * pdfView.zoomLevel);
float yOffset = handlePoint.getY() - (handlePoint.getY() * pdfView.zoomLevel);
pdfView.topLeftOrigin = CGPointMake(origin.getX() + xOffset, origin.getY() + yOffset);

// FIX Fase 2 — MacPdfRenderer non mantiene stato viewport (quello e' di PdfViewComponent)
// La logica corretta va in PdfViewComponent::setCurrentPageZoom:
//
// void PdfViewComponent::setCurrentPageZoom (float newZoom, juce::Point<float> handle)
// {
//     float oldZoom = currentZoom;
//     // 1. Converti il punto di handle da viewport-space a PDF-space
//     float anchorPdfX = (handle.x - topLeft.x) / oldZoom;
//     float anchorPdfY = (handle.y - topLeft.y) / oldZoom;
//     // 2. Calcola il nuovo topLeft che mantiene l'anchor fisso
//     topLeft.x = handle.x - anchorPdfX * newZoom;
//     topLeft.y = handle.y - anchorPdfY * newZoom;
//     currentZoom = newZoom;
//     cachedPageImage = {};  // invalida la cache
//     repaint();
// }
```

Il renderer non gestisce il viewport (zoom/pan) — quello e' compito di `PdfViewComponent`.
`MacPdfRenderer` riceve solo `scale` (fattore di rasterizzazione) e produce un'immagine.

---

## M04 — text extraction e search via PDFKit

Il precedente design basato su `CGPDFScanner` manuale e' **ritirato**. CoreGraphics espone
gli operatori del content stream ma non un text engine completo; implementare correttamente
font Type0, ToUnicode CMap, XObject annidati, writing mode e geometria dei glifi creerebbe un
secondo parser PDF proprietario.

Il backend Apple usa quindi PDFKit, sempre da framework di sistema:

- il modello `PDFDocument` viene creato lazy dai medesimi byte del `CGDataProvider`;
- e' una cache derivata, mai una seconda source of truth;
- `PDFPage.string` implementa `extractText`;
- `NSString rangeOfString:options:range:` implementa ricerca **case-insensitive** pagina per pagina;
- `PDFPage selectionForRange:` produce la selezione geometrica esatta del match;
- `boundsForPage:` e' gia' in page space 72 dpi, lower-left/Y-up: nessuna conversione qui;
- bounds null/non-finite/empty vengono scartate;
- pagina specifica: filtro 0-based validato prima di chiamare PDFKit;
- la cache PDFKit viene rilasciata in `close()` prima del provider/backing bytes.

Le chiamate PDFKit vengono protette al boundary contro eccezioni Objective-C originate da
documenti malformati; le allocazioni C++ vengono gestite senza violare le firme `noexcept`.

---

## Checklist implementazione Fase 2

- [x] `loadFromFile` — implementazione current-only con provider owned e commit transazionale
- [x] `loadFromMemory` — backing buffer owned dal renderer per tutta la vita del provider
- [x] `saveToFile` — snapshot byte-preserving + replace transazionale
- [x] `saveToMemory` — snapshot byte-preserving + publish transazionale
- [x] `getPageCount` — `CGPDFDocumentGetNumberOfPages` con clamp al dominio `int`
- [x] `getPage` — range check 0-based, bounds finite in PDF user space, rotazione normalizzata
- [x] `renderPage` — bounded SoftwareImage + CGBitmapContext + rotation/orientation contract
- [ ] `extractText` — `CGPDFScanner` con operatori Tj/TJ/'/"
- [x] `findText` — PDFKit page-bounded selections, bounds reali in page space
- [x] Nessun `jassertfalse`/TODO residuo nel renderer Apple
- [x] `close()` idempotente: documento -> provider -> backing memory
- [ ] External: rendering asimmetrico + rotazioni 0/90/180/270 + visible box (MediaBox ∩ CropBox) non-zero
- [ ] External: byte equality/round-trip file e memory
- [ ] External: Unicode/Type0/CMap + multiline search + bounds per pagina
