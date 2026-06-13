# Fase 2 — MacPdfRenderer (CoreGraphics)

**Autore**: Ayra Soft  
**Data creazione**: 2026-05-26  
**Stato**: in attesa di implementazione

---

## Obiettivo

Implementare completamente `renderer/mac/ayra_MacPdfRenderer.mm` migrando la logica
CoreGraphics da `pdf_component/Mac_PDF_core/MacPDFComponent.mm`.

Deliverable della Fase 2:
- `loadFromFile` — caricamento da path
- `loadFromMemory` — caricamento da buffer
- `saveToFile` — export su file (migrazione da `exportCurrentDocument`)
- `saveToMemory` — serializzazione in memoria (con fix del bug `dummyURL`)
- `getPageCount` — conteggio pagine strutturale
- `getPage` — informazioni geometriche pagina con conversione Y-axis corretta
- `renderPage` — rasterizzazione in `juce::Image` via `CGBitmapContext`
- `extractText` — estrazione testo tramite `CGPDFScanner`
- `findText` — ricerca testuale con bounding box

---

## Prerequisiti

- Solo macOS e iOS — tutto il file e' sotto `#if JUCE_MAC || JUCE_IOS`
- Nessuna dipendenza esterna — CoreGraphics e' un framework Apple standard
- Framework inclusi automaticamente da JUCE: CoreGraphics.framework, Cocoa.framework (gia' presenti in ayra_pdf.cpp)

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

## Implementazione saveToFile()

Migrazione da `MacPDFViewComponent::exportCurrentDocument()` in `MacPDFComponent.mm:207-234`.

```objc
bool MacPdfRenderer::saveToFile (const juce::File& destFile) const noexcept
{
    if (impl->document == nullptr) { return false; }

    NSString* outputPath = [NSString stringWithUTF8String: destFile.getFullPathName().toRawUTF8()];
    NSURL* outputURL     = [NSURL fileURLWithPath: outputPath];

    CGContextRef ctx = CGPDFContextCreateWithURL ((__bridge CFURLRef) outputURL, nullptr, nullptr);
    if (ctx == nullptr) { return false; }

    size_t numPages = CGPDFDocumentGetNumberOfPages (impl->document);
    for (size_t p = 1; p <= numPages; ++p)
    {
        CGPDFPageRef page = CGPDFDocumentGetPage (impl->document, p);
        CGRect mediaBox   = CGPDFPageGetBoxRect (page, kCGPDFMediaBox);
        CGContextBeginPage (ctx, &mediaBox);
        CGContextDrawPDFPage (ctx, page);
        CGContextEndPage (ctx);
    }

    CGPDFContextClose (ctx);
    CGContextRelease (ctx);
    return true;
}
```

---

## Implementazione saveToMemory() — Fix del bug dummyURL

Il bug in `MacPDFComponent.mm:255-282` usava `CGPDFContextCreateWithURL(@"dummyURL")`
che produce un contesto invalido e non scrive niente. Il pattern corretto richiede
`CGDataConsumerCreate` con callback verso `NSMutableData`.

```objc
bool MacPdfRenderer::saveToMemory (juce::MemoryBlock& destData) const noexcept
{
    if (impl->document == nullptr) { return false; }

    destData.reset();

    // Buffer di accumulo ObjC
    NSMutableData* buffer = [NSMutableData data];

    // Callback per scrivere blocchi nel buffer
    auto putBytesCallback = [] (void* info, const void* buf, size_t count) -> size_t
    {
        NSMutableData* d = (__bridge NSMutableData*) info;
        [d appendBytes: buf length: count];
        return count;
    };

    CGDataConsumerCallbacks callbacks;
    callbacks.putBytes       = putBytesCallback;
    callbacks.releaseConsumer = nullptr;

    CGDataConsumerRef consumer = CGDataConsumerCreate ((__bridge void*) buffer, &callbacks);
    CGContextRef ctx = CGPDFContextCreate (consumer, nullptr, nullptr);
    CGDataConsumerRelease (consumer);

    if (ctx == nullptr) { return false; }

    size_t numPages = CGPDFDocumentGetNumberOfPages (impl->document);
    for (size_t p = 1; p <= numPages; ++p)
    {
        CGPDFPageRef page = CGPDFDocumentGetPage (impl->document, p);
        CGRect mediaBox   = CGPDFPageGetBoxRect (page, kCGPDFMediaBox);
        CGContextBeginPage (ctx, &mediaBox);
        CGContextDrawPDFPage (ctx, page);
        CGContextEndPage (ctx);
    }

    CGPDFContextClose (ctx);
    CGContextRelease (ctx);

    // Trasferimento in MemoryBlock JUCE
    destData.replaceAll (buffer.bytes, (size_t) buffer.length);
    return destData.getSize() > 0;
}
```

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

    CGRect mediaBox = CGPDFPageGetBoxRect (page, kCGPDFMediaBox);
    int rotationCG  = CGPDFPageGetRotationAngle (page); // 0, 90, 180, 270

    PdfPage result;
    result.index  = pageIndex;
    // Bounds in PDF user space (punti tipografici). L'origine e' in basso a sinistra
    // in CoreGraphics, ma bounds.getX()/getY() rappresentano l'offset dell'angolo
    // in alto a sinistra in coordinate juce. Per documenti standard (MediaBox = [0 0 w h])
    // origin.x = 0 e origin.y = 0, quindi nessuna conversione e' necessaria per le dimensioni.
    result.bounds   = { (float)mediaBox.origin.x, (float)mediaBox.origin.y,
                        (float)mediaBox.size.width, (float)mediaBox.size.height };
    result.rotation = rotationCG;
    return result;
}
```

---

## Implementazione renderPage() -> juce::Image

`CGBitmapContext` permette di rasterizzare direttamente sul buffer pixel di `juce::Image`.

Nota: `kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst` produce il formato
`ARGB` su little-endian (Intel e ARM Apple Silicon), compatibile con `juce::Image::ARGB`.

`CGPDFPageGetDrawingTransform` calcola automaticamente la trasformazione che mappa
il PDF user space nel bounds del contesto (gestisce rotazione, scaling, flip Y).

```objc
juce::Image MacPdfRenderer::renderPage (int pageIndex, float scale) noexcept
{
    if (impl->document == nullptr) { return {}; }

    CGPDFPageRef page = CGPDFDocumentGetPage (impl->document, (size_t)(pageIndex + 1));
    if (page == nullptr) { return {}; }

    CGRect mediaBox = CGPDFPageGetBoxRect (page, kCGPDFMediaBox);
    const int w = (int)(mediaBox.size.width  * scale);
    const int h = (int)(mediaBox.size.height * scale);

    if (w <= 0 || h <= 0) { return {}; }

    juce::Image img (juce::Image::ARGB, w, h, true);
    {
        juce::Image::BitmapData bmp (img, juce::Image::BitmapData::writeOnly);

        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef bmpCtx = CGBitmapContextCreate (
            bmp.data,
            (size_t) w, (size_t) h,
            8,                                      // bit per componente
            (size_t) bmp.lineStride,
            cs,
            (CGBitmapInfo)(kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));

        if (bmpCtx != nullptr)
        {
            // Sfondo bianco (i PDF assumono sfondo bianco per default)
            CGContextSetRGBFillColor (bmpCtx, 1.0, 1.0, 1.0, 1.0);
            CGContextFillRect (bmpCtx, CGRectMake (0, 0, w, h));

            // CGPDFPageGetDrawingTransform calcola scale + flip Y
            // (CoreGraphics ha Y-up, CGBitmapContext ha Y-down se la CTM non viene flippata)
            CGRect destRect = CGRectMake (0, 0, w, h);
            CGAffineTransform t = CGPDFPageGetDrawingTransform (
                page, kCGPDFMediaBox, destRect, 0, true);

            CGContextConcatCTM (bmpCtx, t);
            CGContextDrawPDFPage (bmpCtx, page);

            CGContextRelease (bmpCtx);
        }

        CGColorSpaceRelease (cs);
    }

    return img;
}
```

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

## extractText() via CGPDFScanner

CoreGraphics non ha un'API di alto livello per estrarre testo. Si deve implementare tramite
`CGPDFScanner` registrando gli operatori PDF di posizionamento testo (`Tj`, `TJ`, `'`, `"`).

```objc
juce::String MacPdfRenderer::extractText (int pageIndex) noexcept
{
    if (impl->document == nullptr) { return {}; }

    CGPDFPageRef page = CGPDFDocumentGetPage (impl->document, (size_t)(pageIndex + 1));
    if (page == nullptr) { return {}; }

    // Struttura per accumulare il testo estratto
    struct ScanContext
    {
        NSMutableString* text = [[NSMutableString alloc] init];
    };
    ScanContext scanCtx;

    // Helper: decodifica una stringa PDF (puo' essere Latin-1, UTF-16BE o encoding custom)
    auto appendPdfString = [] (CGPDFStringRef pdfStr, NSMutableString* out)
    {
        NSString* decoded = (NSString*) CGPDFStringCopyTextString (pdfStr);
        if (decoded) { [out appendString: decoded]; [decoded release]; }
    };

    CGPDFOperatorTableRef table = CGPDFOperatorTableCreate();

    // Operatore Tj: (string) Tj — mostra una stringa
    CGPDFOperatorTableSetCallback (table, "Tj", [](CGPDFScannerRef scanner, void* info) {
        CGPDFStringRef str = nullptr;
        if (CGPDFScannerPopString (scanner, &str))
        {
            ScanContext* ctx = static_cast<ScanContext*>(info);
            NSString* decoded = (NSString*) CGPDFStringCopyTextString (str);
            if (decoded) { [ctx->text appendString: decoded]; [decoded release]; }
            [ctx->text appendString: @" "]; // separatore tra token
        }
    });

    // Operatore TJ: [(string | adjust)...] TJ — mostra array di stringhe con kerning
    CGPDFOperatorTableSetCallback (table, "TJ", [](CGPDFScannerRef scanner, void* info) {
        CGPDFArrayRef arr = nullptr;
        if (CGPDFScannerPopArray (scanner, &arr))
        {
            ScanContext* ctx = static_cast<ScanContext*>(info);
            size_t count = CGPDFArrayGetCount (arr);
            for (size_t i = 0; i < count; ++i)
            {
                CGPDFStringRef str = nullptr;
                if (CGPDFArrayGetString (arr, i, &str))
                {
                    NSString* decoded = (NSString*) CGPDFStringCopyTextString (str);
                    if (decoded) { [ctx->text appendString: decoded]; [decoded release]; }
                }
                // Gli elementi numerici (kerning) vengono ignorati
            }
            [ctx->text appendString: @" "];
        }
    });

    // Operatori ' e " (mostra stringa con avanzamento riga) — stessa logica di Tj
    CGPDFOperatorTableSetCallback (table, "'", [](CGPDFScannerRef scanner, void* info) {
        CGPDFStringRef str = nullptr;
        if (CGPDFScannerPopString (scanner, &str))
        {
            ScanContext* ctx = static_cast<ScanContext*>(info);
            NSString* decoded = (NSString*) CGPDFStringCopyTextString (str);
            if (decoded) { [ctx->text appendString: decoded]; [decoded release]; }
            [ctx->text appendString: @"\n"];
        }
    });

    CGPDFContentStreamRef stream = CGPDFContentStreamCreateWithPage (page);
    CGPDFScannerRef pdfScanner   = CGPDFScannerCreate (stream, table, &scanCtx);
    CGPDFScannerScan (pdfScanner);
    CGPDFScannerRelease (pdfScanner);
    CGPDFContentStreamRelease (stream);
    CGPDFOperatorTableRelease (table);

    juce::String result = juce::String::fromUTF8 (scanCtx.text.UTF8String);
    [scanCtx.text release];
    return result;
}
```

**Limitazione nota**: `CGPDFStringCopyTextString` gestisce Latin-1, MacRoman e UTF-16BE.
Documenti con encoding custom (Type1 con ToUnicode CMap personalizzata) possono produrre
caratteri sbagliati o interrogativi. PDFium gestisce questo caso meglio.

---

## findText() — ricerca testuale

CoreGraphics non ha API native per la ricerca testuale. L'approccio corretto e' estrarre
il testo per pagina, cercare la stringa, e poi calcolare le bounding box tramite `CGPDFScanner`
tenendo traccia delle posizioni degli operatori di testo.

Una implementazione semplificata che funziona per la maggior parte dei documenti:

```objc
juce::Array<PdfSearchResult> MacPdfRenderer::findText (const juce::String& query, int pageIndex) noexcept
{
    juce::Array<PdfSearchResult> results;
    if (impl->document == nullptr || query.isEmpty()) { return results; }

    int startPage = (pageIndex >= 0) ? pageIndex : 0;
    int endPage   = (pageIndex >= 0) ? pageIndex : getPageCount() - 1;

    for (int p = startPage; p <= endPage; ++p)
    {
        // Estrai il testo e cerca la query
        juce::String pageText = extractText (p);
        if (!pageText.containsIgnoreCase (query)) { continue; }

        // Match trovato: aggiungi un risultato con bounds approssimati (0 = non calcolato)
        // Per bounds precisi servirebbe un secondo scanner che traccia la posizione di ogni glifo.
        // L'implementazione completa e' costosa: richiede il tracking della text matrix (Tm, Td, TD, T*)
        // per ogni operatore e il mapping glifo -> bounds tramite CGPDFFont.
        PdfSearchResult result;
        result.pageIndex = p;
        result.text      = query;
        result.bounds    = {}; // TODO: implementare tracking text matrix per bounds precisi
        results.add (result);
    }

    return results;
}
```

**Nota**: per bounds precisi su macOS sarebbe necessario implementare un CGPDFScanner completo
che traccia la text matrix corrente (operatori `Td`, `TD`, `Tm`, `T*`, `BT`, `ET`) e la
converte in coordinate viewport. Su PDFium questo e' fornito dall'API `FPDFText_GetCharBox`.
Se i bounds precisi di ricerca sono prioritari, valutare l'uso di PDFium anche su macOS.

---

## Checklist implementazione Fase 2

- [ ] `loadFromFile` — migrazione da `MacPDFComponent.mm:63-74`
- [ ] `loadFromMemory` — migrazione da `MacPDFComponent.mm:236-253`
- [ ] `saveToFile` — migrazione da `MacPDFComponent.mm:207-234`
- [ ] `saveToMemory` — **nuovo** con fix `CGDataConsumerCreate` (vedi sezione sopra)
- [ ] `getPageCount` — una riga: `CGPDFDocumentGetNumberOfPages`
- [ ] `getPage` — bounds + rotazione, conversione Y-axis
- [ ] `renderPage` — `CGBitmapContext` + `CGPDFPageGetDrawingTransform`
- [ ] `extractText` — `CGPDFScanner` con operatori Tj/TJ/'/"
- [ ] `findText` — ricerca su testo estratto (bounds approssimati o completi)
- [ ] Rimuovere tutti i `jassertfalse` sostituiti da implementazioni reali
- [ ] Verificare che `close()` rilasci correttamente documento e pdfData
- [ ] Test manuale: aprire un PDF standard, navigare pagine, verificare rendering
- [ ] Test `saveToMemory` -> `loadFromMemory`: round-trip corretto
