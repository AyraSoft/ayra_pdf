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

// Implementazione CoreGraphics di MacPdfRenderer.
//
// Fase 2: migrazione completa da pdf_component/Mac_PDF_core/MacPDFComponent.mm.
// Per ora tutti i metodi sono stub — il vecchio MacPDFViewComponent e' ancora attivo
// e viene incluso in ayra_pdf.cpp per backward compatibility.
//
// Quando la Fase 2 sara' completata:
//   1. Spostare tutta la logica CGPDFDocument da MacPDFComponent.mm qui
//   2. Rimuovere MacPDFViewComponent e aggiornare ayra_pdf.cpp
//   3. PdfViewComponent diventera' il widget unico (backward compat via PdfDocument)

#if JUCE_MAC || JUCE_IOS

namespace ayra
{

// ======================================================================
// Impl — stato ObjC nascosto agli header C++
// ======================================================================

struct MacPdfRenderer::Impl
{
    CGPDFDocumentRef document { nullptr };  ///< Documento CoreGraphics corrente
    juce::MemoryBlock pdfData;              ///< Copia del PDF in memoria per CGDataProvider

    // TODO: Fase 2 — aggiungere CGDataProviderRef, cache pagine, mutex se necessario
};

// ======================================================================
// MacPdfRenderer
// ======================================================================

MacPdfRenderer::MacPdfRenderer()
    : impl (std::make_unique<Impl>()) {}

MacPdfRenderer::~MacPdfRenderer()
{
    close();
}

bool MacPdfRenderer::loadFromFile (const juce::File& file) noexcept
{
    jassertfalse; // TODO: Fase 2 — migrare da MacPDFComponent.mm loadDocument()
    return false;
}

bool MacPdfRenderer::loadFromMemory (const void* data, size_t sizeBytes) noexcept
{
    jassertfalse; // TODO: Fase 2 — migrare da MacPDFComponent.mm loadDocumentFromMemoryBlock()
    return false;
}

bool MacPdfRenderer::saveToFile (const juce::File& destFile) const noexcept
{
    jassertfalse; // TODO: Fase 2 — migrare da MacPDFComponent.mm exportCurrentDocument()
    return false;
}

bool MacPdfRenderer::saveToMemory (juce::MemoryBlock& destData) const noexcept
{
    jassertfalse; // TODO: Fase 2 — migrare da MacPDFComponent.mm getMemoryBlockFromDocument()
    return false;
}

void MacPdfRenderer::close() noexcept
{
    if (impl->document != nullptr)
    {
        CGPDFDocumentRelease (impl->document);
        impl->document = nullptr;
    }
    impl->pdfData.reset();
}

bool MacPdfRenderer::isLoaded() const noexcept
{
    return impl->document != nullptr;
}

int MacPdfRenderer::getPageCount() const noexcept
{
    if (impl->document == nullptr) { return 0; }
    // TODO: Fase 2 — return (int)CGPDFDocumentGetNumberOfPages(impl->document);
    return 0;
}

PdfPage MacPdfRenderer::getPage (int pageIndex) const noexcept
{
    jassertfalse; // TODO: Fase 2
    return {};
}

juce::Image MacPdfRenderer::renderPage (int pageIndex, float scale) noexcept
{
    jassertfalse; // TODO: Fase 2
    return {};
}

juce::Array<PdfSearchResult> MacPdfRenderer::findText (const juce::String& query, int pageIndex) noexcept
{
    jassertfalse; // TODO: Fase 2
    return {};
}

juce::String MacPdfRenderer::extractText (int pageIndex) noexcept
{
    jassertfalse; // TODO: Fase 2
    return {};
}

} // namespace ayra

#endif // JUCE_MAC || JUCE_IOS
