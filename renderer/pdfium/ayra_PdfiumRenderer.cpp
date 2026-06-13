/*
     ______  __    __  ______   ______
    |      \|  \  |  \/      \ |      \
     \######\ ##  | ##  ######\ \######\
     /      ## ##  | ## ##   \##/      ##   Copyright 2023-2026
    |  #######\ ##__/ ## ##     |  #######   Ayra Soft
     \##    ##\##    ## ##      \##    ##   www.ayra.live
      \#######_\######\##       \#######
             |  \__| ##
              \##    ##
                \######

 Ayra uses a GPL/commercial licence - see LICENCE.md for details.
*/

// Implementazione PDFium di PdfiumRenderer.
//
// Fase 3: implementazione completa.
//
// Prerequisiti:
//   - Header PDFium in:   third_party/pdfium/include/
//   - Binari per Windows: third_party/pdfium/win/pdfium.lib + pdfium.dll
//   - Binari per Linux:   third_party/pdfium/linux/libpdfium.a (o .so)
//   - Binari per Android: third_party/pdfium/android/<abi>/libpdfium.so
//
// Setup automatico binari: ./scripts/setup_pdfium.sh
//
// Documentazione API PDFium: https://pdfium.googlesource.com/pdfium/

#if JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID

// Include PDFium solo se gli header sono presenti
// (guard: non fallire la compilazione se PDFium non e' ancora installato)
#if __has_include("../../third_party/pdfium/include/fpdfview.h")
  #include "../../third_party/pdfium/include/fpdfview.h"
  #include "../../third_party/pdfium/include/fpdf_text.h"
  #include "../../third_party/pdfium/include/fpdf_save.h"
  #define AYRA_PDFIUM_AVAILABLE 1
#endif

namespace ayra
{

// ======================================================================
// Impl — stato PDFium nascosto agli header C++
// ======================================================================

struct PdfiumRenderer::Impl
{
#ifdef AYRA_PDFIUM_AVAILABLE
    FPDF_DOCUMENT document { nullptr };  ///< Documento PDFium corrente
#endif
    juce::MemoryBlock pdfData;           ///< Copia del PDF in memoria (richiesta da PDFium per FPDF_LoadMemDocument)

    // TODO: Fase 3 — aggiungere cache pagine, stato inizializzazione PDFium
};

// ======================================================================
// PdfiumRenderer
// ======================================================================

PdfiumRenderer::PdfiumRenderer()
    : impl (std::make_unique<Impl>())
{
#ifdef AYRA_PDFIUM_AVAILABLE
    // TODO: Fase 3 — FPDF_InitLibrary() (singleton — usare reference count)
#endif
}

PdfiumRenderer::~PdfiumRenderer()
{
    close();
#ifdef AYRA_PDFIUM_AVAILABLE
    // TODO: Fase 3 — FPDF_DestroyLibrary() (solo quando ref count == 0)
#endif
}

bool PdfiumRenderer::loadFromFile (const juce::File& file) noexcept
{
    jassertfalse; // TODO: Fase 3 — FPDF_LoadDocument(path, password)
    return false;
}

bool PdfiumRenderer::loadFromMemory (const void* data, size_t sizeBytes) noexcept
{
    jassertfalse; // TODO: Fase 3 — FPDF_LoadMemDocument(data, size, password)
    return false;
}

bool PdfiumRenderer::saveToFile (const juce::File& destFile) const noexcept
{
    jassertfalse; // TODO: Fase 3 — FPDF_SaveAsCopy con FPDF_FILEWRITE
    return false;
}

bool PdfiumRenderer::saveToMemory (juce::MemoryBlock& destData) const noexcept
{
    jassertfalse; // TODO: Fase 3 — FPDF_SaveAsCopy su MemoryBlock custom writer
    return false;
}

void PdfiumRenderer::close() noexcept
{
#ifdef AYRA_PDFIUM_AVAILABLE
    if (impl->document != nullptr)
    {
        // TODO: Fase 3 — FPDF_CloseDocument(impl->document);
        impl->document = nullptr;
    }
#endif
    impl->pdfData.reset();
}

bool PdfiumRenderer::isLoaded() const noexcept
{
#ifdef AYRA_PDFIUM_AVAILABLE
    return impl->document != nullptr;
#else
    return false;
#endif
}

int PdfiumRenderer::getPageCount() const noexcept
{
#ifdef AYRA_PDFIUM_AVAILABLE
    // TODO: Fase 3 — return FPDF_GetPageCount(impl->document);
#endif
    return 0;
}

PdfPage PdfiumRenderer::getPage (int pageIndex) const noexcept
{
    jassertfalse; // TODO: Fase 3 — FPDF_GetPageWidth/Height, FPDF_GetPageRotation
    return {};
}

juce::Image PdfiumRenderer::renderPage (int pageIndex, float scale) noexcept
{
    jassertfalse; // TODO: Fase 3 — FPDF_RenderPageBitmap su buffer JUCE
    return {};
}

juce::Array<PdfSearchResult> PdfiumRenderer::findText (const juce::String& query, int pageIndex) noexcept
{
    jassertfalse; // TODO: Fase 3 — FPDFText_FindStart/FindNext
    return {};
}

juce::String PdfiumRenderer::extractText (int pageIndex) noexcept
{
    jassertfalse; // TODO: Fase 3 — FPDFText_LoadPage + FPDFText_GetText
    return {};
}

} // namespace ayra

#endif // JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID
