# Fase 3 — PdfiumRenderer (PDFium)

**Autore**: Ayra Soft  
**Data creazione**: 2026-05-26  
**Stato operativo**: vedere `_docs/02_stato_attuale.md` (source of truth)

---

## Prerequisiti

- PDFium binario installato/linkato per la piattaforma target; gli header pubblici sono vendorizzati in git
- Platform guard: tutto il file e' sotto `#if JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID`
- Header usati: `fpdfview.h`, `fpdf_edit.h`, `fpdf_transformpage.h`, poi `fpdf_text.h`/`fpdf_save.h` in P02/P03
- **Nessun fallback `__has_include`**: se il target dichiara PDFium ma il binary/link manca, il build deve fallire invece di produrre un backend inert
- Linker/deployment: vedi `07_deployment.md`

---

## Init PDFium — lifecycle process-wide serializzato

L'header vendorizzato `fpdfview.h` e l'upstream PDFium dichiarano che **nessuna API PDFium
e' thread-safe**. Non basta quindi evitare accesso concorrente allo stesso documento: anche
istanze diverse devono essere serializzate.

P01 usa un unico `juce::CriticalSection` process-wide come protocollo canonico per **tutte** le chiamate
FPDF. Lo stesso stato mantiene un instance count:

- 0 -> 1: `FPDF_InitLibraryWithConfig`;
- 1 -> 0: `FPDF_DestroyLibrary`;
- ogni public method che tocca FPDF acquisisce lo stesso mutex;
- niente `std::once_flag`: non e' compatibile con destroy + eventuale re-init.

La configurazione e' value-initialized, `version = 2`, font path default, isolate nullo.
Il mutex non rende sicuro distruggere un oggetto mentre un altro thread lo usa: il lifetime
dell'istanza resta un invariant del caller.

---

## loadFromFile() — converge sul percorso memory

Non viene usato `FPDF_LoadDocument(path)`: mantenere due loader produrrebbe due policy di
lifetime/save/error handling. Il file viene validato e letto con JUCE entro
`detail::maxDocumentBytes`, poi passa allo stesso percorso owned-memory di `loadFromMemory`.

La lettura file avviene fuori dal mutex PDFium; solo la chiamata FPDF e il commit del nuovo
documento sono serializzati. Un load fallito lascia intatto il documento precedente.

---

## loadFromMemory()

`FPDF_LoadMemDocument64` mantiene il buffer valido per la vita del documento. P01:

1. valida null/size e il limite canonico;
2. copia in un `MemoryBlock` owned fuori dalla lock PDFium;
3. acquisisce il mutex process-wide;
4. crea il nuovo documento;
5. solo a successo chiude il precedente e pubblica documento + backing buffer.

Questo rende il load transazionale e rimuove narrowing a `int`.

---

## saveToFile() / saveToMemory() — byte-preserving

P01 carica sempre da backing bytes owned, anche quando l'origine e' un file. Il backend e'
read-only: P02 salva quindi quei byte direttamente, come il backend Apple.

- niente `FPDF_SaveAsCopy`;
- snapshot `MemoryBlock` sotto la lock che protegge lo stato dell'istanza;
- `saveToFile`: I/O tramite `replaceWithData` dopo aver rilasciato la lock PDFium;
- `saveToMemory`: replacement costruito prima della pubblicazione;
- fallimento di allocazione/I/O non produce output parziale.

---

## renderPage() -> juce::Image

PDFium renderizza direttamente nel backing buffer di un `juce::SoftwareImageType`.

Contratto corrente:
- `FPDF_GetPageWidthF/HeightF` produce la display size gia' coerente con la rotazione della pagina;
- `FPDF_RenderPageBitmap` riceve `rotate = 0`, quindi non aggiunge una seconda rotazione;
- `FPDFBitmap_BGRA` coincide con `juce::PixelARGB` su Windows/Linux little-endian;
- su Android il layout JUCE e' RGB[A], quindi si usa `FPDF_REVERSE_BYTE_ORDER`;
- nessun loop di swap post-render;
- backdrop bianco;
- niente `FPDF_ANNOT`: il contratto corrente renderizza page content, come CoreGraphics;
- `FPDF_RENDER_LIMITEDIMAGECACHE` limita la cache immagini del render;
- scale, dimensioni, pixel count, stride e buffer size sono validati con gli stessi limiti M02.

La verifica runtime deve usare color bars + marker ai quattro angoli + /Rotate 0/90/180/270.

---

## findText()

PDFium usa coordinate con Y-up (origine in basso a sinistra della pagina). `PdfSearchResult.bounds`
usa PDF user space (punti, origine in basso a sinistra). La conversione Y non e' necessaria se
manteniamo le coordinate in PDF user space — e' `PdfViewComponent` che le convertira' in
screen-space durante la visualizzazione.

```cpp
juce::Array<PdfSearchResult> PdfiumRenderer::findText (const juce::String& query, int pageIndex) noexcept
{
#ifndef AYRA_PDFIUM_AVAILABLE
    return {};
#else
    juce::Array<PdfSearchResult> results;
    if (impl->document == nullptr || query.isEmpty()) { return results; }

    const int startPage = (pageIndex >= 0) ? pageIndex : 0;
    const int endPage   = (pageIndex >= 0) ? pageIndex : FPDF_GetPageCount (impl->document) - 1;

    // Converti la query in UTF-16LE per PDFium
    juce::MemoryBlock utf16Query;
    const juce::CharPointer_UTF16 queryUtf16 = query.toUTF16();
    // UTF-16 terminator e' 2 byte (0x0000)
    size_t utf16Len = 0;
    for (auto p = queryUtf16; *p != 0; ++p) { ++utf16Len; }
    utf16Query.replaceAll (queryUtf16.getAddress(), (utf16Len + 1) * sizeof (juce::juce_wchar));

    for (int p = startPage; p <= endPage; ++p)
    {
        FPDF_PAGE page = FPDF_LoadPage (impl->document, p);
        if (page == nullptr) { continue; }

        FPDF_TEXTPAGE textPage = FPDFText_LoadPage (page);
        if (textPage == nullptr) { FPDF_ClosePage (page); continue; }

        FPDF_SCHHANDLE search = FPDFText_FindStart (
            textPage,
            reinterpret_cast<const unsigned short*>(utf16Query.getData()),
            0,   // flags = 0 (case-insensitive)
            0);  // startIndex = 0

        while (FPDFText_FindNext (search))
        {
            const int matchIndex = FPDFText_GetSchResultIndex (search);
            const int matchCount = FPDFText_GetSchCount (search);

            // Calcola bounding box come unione delle char box
            double left = 1e9, top = -1e9, right = -1e9, bottom = 1e9;
            for (int i = matchIndex; i < matchIndex + matchCount; ++i)
            {
                double cl, ct, cr, cb;
                FPDFText_GetCharBox (textPage, i, &cl, &cr, &cb, &ct);
                // Nota: PDFium inverte top/bottom rispetto a cosa ci si aspetta
                // cl = left, cr = right, cb = bottom (y-up), ct = top (y-up)
                left   = std::min (left,   cl);
                bottom = std::min (bottom, cb);
                right  = std::max (right,  cr);
                top    = std::max (top,    ct);
            }

            PdfSearchResult result;
            result.pageIndex = p;
            result.text      = query;
            // Bounds in PDF user space (Y-up, origine in basso a sinistra)
            result.bounds    = { (float)left,   (float)bottom,
                                 (float)(right - left), (float)(top - bottom) };
            results.add (result);
        }

        FPDFText_FindClose (search);
        FPDFText_ClosePage (textPage);
        FPDF_ClosePage (page);
    }

    return results;
#endif
}
```

---

## extractText()

`FPDFText_GetText` produce UTF-16LE. `juce::String` puo' essere costruito da UTF-16
tramite `juce::CharPointer_UTF16`.

```cpp
juce::String PdfiumRenderer::extractText (int pageIndex) noexcept
{
#ifndef AYRA_PDFIUM_AVAILABLE
    return {};
#else
    if (impl->document == nullptr) { return {}; }

    FPDF_PAGE page = FPDF_LoadPage (impl->document, pageIndex);
    if (page == nullptr) { return {}; }

    FPDF_TEXTPAGE textPage = FPDFText_LoadPage (page);
    if (textPage == nullptr) { FPDF_ClosePage (page); return {}; }

    const int charCount = FPDFText_CountChars (textPage);
    if (charCount <= 0)
    {
        FPDFText_ClosePage (textPage);
        FPDF_ClosePage (page);
        return {};
    }

    // Buffer UTF-16LE: (charCount + 1) caratteri * 2 byte/char
    juce::HeapBlock<unsigned short> buf ((size_t)(charCount + 1));
    FPDFText_GetText (textPage, 0, charCount, buf.getData());
    buf[charCount] = 0; // null terminator

    FPDFText_ClosePage (textPage);
    FPDF_ClosePage (page);

    return juce::String (juce::CharPointer_UTF16 (
        reinterpret_cast<const juce::CharPointer_UTF16::CharType*>(buf.getData())));
#endif
}
```

---

## getPageCount()

```cpp
int PdfiumRenderer::getPageCount() const noexcept
{
#ifndef AYRA_PDFIUM_AVAILABLE
    return 0;
#else
    if (impl->document == nullptr) { return 0; }
    return FPDF_GetPageCount (impl->document);
#endif
}
```

---

## getPage()

```cpp
PdfPage PdfiumRenderer::getPage (int pageIndex) const noexcept
{
#ifndef AYRA_PDFIUM_AVAILABLE
    return {};
#else
    if (impl->document == nullptr) { return {}; }

    FPDF_PAGE page = FPDF_LoadPage (impl->document, pageIndex);
    if (page == nullptr) { return {}; }

    PdfPage result;
    result.index    = pageIndex;
    result.bounds   = { 0.f, 0.f,
                        (float) FPDF_GetPageWidth  (page),
                        (float) FPDF_GetPageHeight (page) };
    // FPDFPage_GetRotation ritorna 0,1,2,3 corrispondenti a 0,90,180,270 gradi
    result.rotation = FPDFPage_GetRotation (page) * 90;

    FPDF_ClosePage (page);
    return result;
#endif
}
```

---

## Sistema di coordinate — IMPORTANTE

PDFium usa:
- Origine in basso a sinistra della pagina
- Y cresce verso l'alto (Y-up)
- Unita' di misura: PDF user space points (1/72 pollice)

juce::Image usa:
- Origine in alto a sinistra
- Y cresce verso il basso (Y-down)

**`renderPage()`**: PDFium gestisce il flip Y internamente durante `FPDF_RenderPageBitmap`.
Il buffer prodotto ha Y-down (compatibile con juce::Image). Nessuna conversione necessaria.

**`findText()` bounding box**: i bounds in `PdfSearchResult` sono in PDF user space (Y-up).
`PdfViewComponent` deve convertirli in screen-space prima di disegnarli. Formula:
```cpp
float screenY = pageHeightPoints - (pdfBounds.getY() + pdfBounds.getHeight());
float screenH = pdfBounds.getHeight();
juce::Rectangle<float> screenBounds (pdfBounds.getX() * zoom + topLeft.x,
                                      screenY            * zoom + topLeft.y,
                                      pdfBounds.getWidth()  * zoom,
                                      screenH               * zoom);
```

**`getPage().bounds`**: bounds in PDF user space (Y-up). Usato da `PdfViewComponent` per
calcolare le dimensioni del documento in punti — la conversione Y avviene nel widget.

---

## close()

```cpp
void PdfiumRenderer::close() noexcept
{
#ifdef AYRA_PDFIUM_AVAILABLE
    if (impl->document != nullptr)
    {
        FPDF_CloseDocument (impl->document);
        impl->document = nullptr;
    }
#endif
    impl->pdfData.reset();
}
```

---

## Checklist implementazione Fase 3

- [ ] Verificare che `setup_pdfium.sh` abbia scaricato i binari per la piattaforma target
- [x] Rimosso fallback `AYRA_PDFIUM_AVAILABLE/__has_include`: backend/link obbligatori
- [x] `loadFromFile` — bounded JUCE read -> percorso memory canonico
- [x] `loadFromMemory` — `FPDF_LoadMemDocument64`, commit transazionale
- [ ] `saveToFile` — snapshot byte-preserving + `replaceWithData`
- [ ] `saveToMemory` — snapshot byte-preserving transazionale
- [x] `close` — `FPDF_CloseDocument` prima del reset backing data
- [x] `getPageCount` — `FPDF_GetPageCount` serializzato
- [x] `getPage` — `FPDFPage_GetMediaBox` + `FPDFPage_GetRotation`
- [ ] `renderPage` — bounded direct bitmap; Android reverse-byte-order flag
- [ ] `extractText` — `FPDFText_LoadPage` + `FPDFText_GetText` + UTF-16LE -> juce::String
- [ ] `findText` — `FPDFText_FindStart/Next/Close` + `FPDFText_GetCharBox`
- [ ] Rimuovere tutti i `jassertfalse` sostituiti da implementazioni reali
- [x] Lifecycle + lock process-wide su ogni API PDFium; init 0->1 / destroy 1->0
- [ ] Test manuale Windows: aprire PDF, renderizzare pagina, verificare colori corretti
- [ ] Test round-trip: `loadFromFile` -> `saveToMemory` -> `loadFromMemory` -> `renderPage` deve produrre immagine identica
