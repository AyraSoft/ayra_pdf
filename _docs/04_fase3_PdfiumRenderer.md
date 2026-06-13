# Fase 3 — PdfiumRenderer (PDFium)

**Autore**: Ayra Soft  
**Data creazione**: 2026-05-26  
**Stato**: in attesa di implementazione

---

## Prerequisiti

- PDFium installato via `./scripts/setup_pdfium.sh` (o `setup_pdfium.ps1` su Windows)
- Platform guard: tutto il file e' sotto `#if JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID`
- Header disponibili: `third_party/pdfium/include/fpdfview.h`, `fpdf_text.h`, `fpdf_save.h`
- La guard `#if __has_include(...)` in `ayra_PdfiumRenderer.cpp` permette di compilare anche
  senza PDFium installato (metodi ritornano `false` / immagine invalida senza crash)
- Linker flags per piattaforma: vedi `07_deployment.md`

---

## Init PDFium — singleton per processo

PDFium richiede `FPDF_InitLibraryWithConfig` una sola volta per processo (documentato in
`fpdfview.h`). Nessuna API e' thread-safe: serializzare se necessario con mutex esterno.

```cpp
namespace {
    std::once_flag   g_pdfiumInitFlag;
    std::atomic<int> g_pdfiumInstanceCount { 0 };

    void initPdfiumLibrary()
    {
        FPDF_LIBRARY_CONFIG cfg;
        cfg.version          = 2;
        cfg.m_pUserFontPaths = nullptr;  // usa i font di sistema
        cfg.m_pIsolate       = nullptr;  // nessun V8 isolate (non usiamo JS)
        cfg.m_v8EmbedderSlot = 0;
        FPDF_InitLibraryWithConfig (&cfg);
    }
}

PdfiumRenderer::PdfiumRenderer() : impl (std::make_unique<Impl>())
{
#ifdef AYRA_PDFIUM_AVAILABLE
    std::call_once (g_pdfiumInitFlag, initPdfiumLibrary);
    g_pdfiumInstanceCount.fetch_add (1, std::memory_order_relaxed);
#endif
}

PdfiumRenderer::~PdfiumRenderer()
{
    close();
#ifdef AYRA_PDFIUM_AVAILABLE
    if (g_pdfiumInstanceCount.fetch_sub (1, std::memory_order_acq_rel) == 1)
        FPDF_DestroyLibrary();
        // Reset per permettere re-init in test unitari che creano/distruggono renderer piu' volte:
        // g_pdfiumInitFlag = std::once_flag{}; // NON thread-safe — usare solo in test mono-thread
#endif
}
```

---

## loadFromFile()

```cpp
bool PdfiumRenderer::loadFromFile (const juce::File& file) noexcept
{
#ifndef AYRA_PDFIUM_AVAILABLE
    return false;
#else
    close();
    impl->document = FPDF_LoadDocument (file.getFullPathName().toRawUTF8(), nullptr);
    return impl->document != nullptr;
#endif
}
```

---

## loadFromMemory()

**CRITICO**: `FPDF_LoadMemDocument` mantiene un puntatore al buffer originale per tutta la vita
del documento. Il buffer deve rimanere valido finche' il documento non viene chiuso.
Lo salviamo in `impl->pdfData` per garantire la lifetime.

```cpp
bool PdfiumRenderer::loadFromMemory (const void* data, size_t sizeBytes) noexcept
{
#ifndef AYRA_PDFIUM_AVAILABLE
    return false;
#else
    close();
    impl->pdfData.replaceAll (data, sizeBytes); // copia nel buffer owned
    impl->document = FPDF_LoadMemDocument (
        impl->pdfData.getData(),
        (int) impl->pdfData.getSize(),
        nullptr); // nullptr = nessuna password
    if (impl->document == nullptr) { impl->pdfData.reset(); }
    return impl->document != nullptr;
#endif
}
```

---

## saveToFile()

```cpp
bool PdfiumRenderer::saveToFile (const juce::File& destFile) const noexcept
{
#ifndef AYRA_PDFIUM_AVAILABLE
    return false;
#else
    if (impl->document == nullptr) { return false; }

    // Pattern FPDF_FILEWRITE per scrittura su file
    struct FileWriter
    {
        FPDF_FILEWRITE base; // deve essere il primo campo (C ABI struct extension)
        FILE* fp;

        static int writeBlock (FPDF_FILEWRITE* self, const void* data, unsigned long sz)
        {
            auto* fw = reinterpret_cast<FileWriter*>(self);
            return (fwrite (data, 1, (size_t)sz, fw->fp) == (size_t)sz) ? 1 : 0;
        }
    };

    FILE* fp = fopen (destFile.getFullPathName().toRawUTF8(), "wb");
    if (fp == nullptr) { return false; }

    FileWriter writer;
    writer.base.version    = 1;
    writer.base.WriteBlock = FileWriter::writeBlock;
    writer.fp              = fp;

    FPDF_BOOL result = FPDF_SaveAsCopy (impl->document,
                                        reinterpret_cast<FPDF_FILEWRITE*>(&writer), 0);
    fclose (fp);
    if (!result) { destFile.deleteFile(); }
    return result != 0;
#endif
}
```

---

## saveToMemory()

```cpp
bool PdfiumRenderer::saveToMemory (juce::MemoryBlock& destData) const noexcept
{
#ifndef AYRA_PDFIUM_AVAILABLE
    return false;
#else
    if (impl->document == nullptr) { return false; }

    destData.reset();

    struct MemWriter
    {
        FPDF_FILEWRITE  base;    // primo campo — C ABI
        juce::MemoryBlock* dest;

        static int writeBlock (FPDF_FILEWRITE* self, const void* data, unsigned long sz)
        {
            auto* mw = reinterpret_cast<MemWriter*>(self);
            mw->dest->append (data, (size_t)sz);
            return 1; // successo
        }
    };

    MemWriter writer;
    writer.base.version    = 1;
    writer.base.WriteBlock = MemWriter::writeBlock;
    writer.dest            = &destData;

    return FPDF_SaveAsCopy (impl->document,
                            reinterpret_cast<FPDF_FILEWRITE*>(&writer), 0) != 0;
#endif
}
```

---

## renderPage() -> juce::Image

PDFium produce pixel in formato `BGRA` (Blue, Green, Red, Alpha). `juce::Image::ARGB`
usa il formato `ARGB` (Alpha, Red, Green, Blue) in memoria. E' necessario swappare B e R.

`FPDFBitmap_CreateEx` con `FPDFBitmap_BGRA` usa il buffer esterno di `juce::Image::BitmapData`
direttamente (niente copia intermedia). Lo swap B<->R avviene in-place dopo il rendering.

```cpp
juce::Image PdfiumRenderer::renderPage (int pageIndex, float scale) noexcept
{
#ifndef AYRA_PDFIUM_AVAILABLE
    return {};
#else
    if (impl->document == nullptr) { return {}; }

    FPDF_PAGE page = FPDF_LoadPage (impl->document, pageIndex);
    if (page == nullptr) { return {}; }

    const int w = (int)(FPDF_GetPageWidth  (page) * scale);
    const int h = (int)(FPDF_GetPageHeight (page) * scale);

    if (w <= 0 || h <= 0) { FPDF_ClosePage (page); return {}; }

    juce::Image img (juce::Image::ARGB, w, h, true);
    {
        juce::Image::BitmapData bmp (img, juce::Image::BitmapData::writeOnly);

        // Usa il buffer di juce::Image come backing store per PDFium (zero copia)
        FPDF_BITMAP bitmap = FPDFBitmap_CreateEx (
            w, h,
            FPDFBitmap_BGRA,        // formato PDFium
            bmp.data,               // buffer diretto
            (int) bmp.lineStride);  // stride in byte

        FPDFBitmap_FillRect (bitmap, 0, 0, w, h, 0xFFFFFFFF); // sfondo bianco

        FPDF_RenderPageBitmap (
            bitmap, page,
            0, 0, w, h,             // offset e dimensioni nel bitmap
            0,                      // rotazione (0 = nessuna)
            FPDF_ANNOT | FPDF_LCD_TEXT); // flag: renderizza annotazioni + ClearType

        FPDFBitmap_Destroy (bitmap);

        // Swap BGRA -> ARGB in-place (B e R sono in posizioni diverse)
        // BGRA in memoria: [B][G][R][A]
        // ARGB in memoria: [A][R][G][B]  (ma juce::Image::ARGB su little-endian = [B][G][R][A])
        // In realta' su little-endian, juce::Image::ARGB == FPDFBitmap_BGRA -> nessun swap necessario
        // Verificare con: Image::BitmapData bmp(img, ...); bmp.getPixelColour(0,0).getRed() == atteso
        //
        // Se lo swap e' necessario (macchine big-endian o configurazioni diverse):
        for (int y = 0; y < h; ++y)
        {
            auto* px = reinterpret_cast<uint8_t*>(bmp.getLinePointer (y));
            for (int x = 0; x < w; ++x, px += 4)
                std::swap (px[0], px[2]); // swap B e R
        }
    }

    FPDF_ClosePage (page);
    return img;
#endif
}
```

**Nota sullo swap B/R**: PDFium e juce::Image usano entrambi BGRA su little-endian (x86, ARM).
Lo swap potrebbe non essere necessario. Verificare con un documento di test (pixel rosso puro
`#FF0000`) e controllare che `img.getPixelAt(x,y).getRed() == 255`. Se il risultato e' `getBlue() == 255`,
lo swap e' necessario. Se il rendering e' corretto senza swap, rimuovere il loop per performance.

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
- [ ] Aggiornare `AYRA_PDFIUM_AVAILABLE` guard se necessario (attualmente in `ayra_PdfiumRenderer.cpp`)
- [ ] `loadFromFile` — `FPDF_LoadDocument`
- [ ] `loadFromMemory` — `FPDF_LoadMemDocument` con copia in `impl->pdfData`
- [ ] `saveToFile` — `FPDF_SaveAsCopy` con `FileWriter` struct
- [ ] `saveToMemory` — `FPDF_SaveAsCopy` con `MemWriter` struct
- [ ] `close` — `FPDF_CloseDocument` + reset pdfData
- [ ] `getPageCount` — `FPDF_GetPageCount`
- [ ] `getPage` — `FPDF_GetPageWidth/Height`, `FPDFPage_GetRotation`
- [ ] `renderPage` — `FPDFBitmap_CreateEx` + `FPDF_RenderPageBitmap` + swap B/R se necessario
- [ ] `extractText` — `FPDFText_LoadPage` + `FPDFText_GetText` + UTF-16LE -> juce::String
- [ ] `findText` — `FPDFText_FindStart/Next/Close` + `FPDFText_GetCharBox`
- [ ] Rimuovere tutti i `jassertfalse` sostituiti da implementazioni reali
- [ ] Verificare lifecycle: costruttore init + distruttore decrement + destroy quando last instance
- [ ] Test manuale Windows: aprire PDF, renderizzare pagina, verificare colori corretti
- [ ] Test round-trip: `loadFromFile` -> `saveToMemory` -> `loadFromMemory` -> `renderPage` deve produrre immagine identica
