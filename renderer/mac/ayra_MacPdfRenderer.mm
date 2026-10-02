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
// Roadmap canonica: _docs/02_stato_attuale.md.
// M01 implementa lifecycle, caricamento e metadati pagina.
// I successivi M02-M04 completano rendering, serializzazione e testo/ricerca.
// Il codice legacy e' solo riferimento storico e verra' rimosso al cutover current-only.

#if JUCE_MAC || JUCE_IOS

#include <cmath>
#include <limits>
#include <memory>

namespace ayra
{

// ======================================================================
// Impl — stato ObjC nascosto agli header C++
// ======================================================================

struct MacPdfRenderer::Impl
{
    CGPDFDocumentRef document { nullptr };        ///< Documento CoreGraphics corrente
    CGDataProviderRef provider { nullptr };       ///< Provider mantenuto vivo quanto il documento
    std::unique_ptr<juce::MemoryBlock> pdfData;   ///< Backing storage per load-from-memory
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
    if (!file.existsAsFile())
        return false;

    const auto path = file.getFullPathName();
    CGDataProviderRef provider = CGDataProviderCreateWithFilename (path.toRawUTF8());

    if (provider == nullptr)
        return false;

    CGPDFDocumentRef newDocument = CGPDFDocumentCreateWithProvider (provider);

    if (newDocument == nullptr)
    {
        CGDataProviderRelease (provider);
        return false;
    }

    // Commit transazionale: un load fallito lascia intatto il documento corrente.
    close();
    impl->provider = provider;
    impl->document = newDocument;
    return true;
}

bool MacPdfRenderer::loadFromMemory (const void* data, size_t sizeBytes) noexcept
{
    if (data == nullptr || sizeBytes == 0)
        return false;

    auto newPdfData = std::make_unique<juce::MemoryBlock>();
    newPdfData->replaceAll (data, sizeBytes);

    CGDataProviderRef provider = CGDataProviderCreateWithData (nullptr,
                                                               newPdfData->getData(),
                                                               newPdfData->getSize(),
                                                               nullptr);

    if (provider == nullptr)
        return false;

    CGPDFDocumentRef newDocument = CGPDFDocumentCreateWithProvider (provider);

    if (newDocument == nullptr)
    {
        CGDataProviderRelease (provider);
        return false;
    }

    // Provider e backing storage restano entrambi owned dal renderer per tutta la vita
    // del documento. Nessun puntatore ai byte sopravvive al proprio owner.
    close();
    impl->pdfData = std::move (newPdfData);
    impl->provider = provider;
    impl->document = newDocument;
    return true;
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

    if (impl->provider != nullptr)
    {
        CGDataProviderRelease (impl->provider);
        impl->provider = nullptr;
    }

    impl->pdfData.reset();
}

bool MacPdfRenderer::isLoaded() const noexcept
{
    return impl->document != nullptr;
}

int MacPdfRenderer::getPageCount() const noexcept
{
    if (impl->document == nullptr)
        return 0;

    const auto count = CGPDFDocumentGetNumberOfPages (impl->document);
    constexpr auto maxInt = static_cast<size_t> (std::numeric_limits<int>::max());

    return count > maxInt ? std::numeric_limits<int>::max()
                          : static_cast<int> (count);
}

PdfPage MacPdfRenderer::getPage (int pageIndex) const noexcept
{
    if (impl->document == nullptr || pageIndex < 0)
        return {};

    const int addressablePageCount = getPageCount();

    if (pageIndex >= addressablePageCount)
        return {};

    const auto pageNumber = static_cast<size_t> (pageIndex) + 1u;

    CGPDFPageRef page = CGPDFDocumentGetPage (impl->document, pageNumber);

    if (page == nullptr)
        return {};

    const auto mediaBox = CGRectStandardize (CGPDFPageGetBoxRect (page, kCGPDFMediaBox));

    const auto fitsFloat = [] (CGFloat value) noexcept
    {
        const auto d = static_cast<double> (value);
        constexpr auto maxFloat = static_cast<double> (std::numeric_limits<float>::max());
        return std::isfinite (d) && d >= -maxFloat && d <= maxFloat;
    };

    if (CGRectIsNull (mediaBox) || CGRectIsEmpty (mediaBox) || CGRectIsInfinite (mediaBox)
        || !fitsFloat (mediaBox.origin.x) || !fitsFloat (mediaBox.origin.y)
        || !fitsFloat (mediaBox.size.width) || !fitsFloat (mediaBox.size.height))
    {
        return {};
    }

    const int rawRotation = CGPDFPageGetRotationAngle (page);
    const int rotation = ((rawRotation % 360) + 360) % 360;

    if ((rotation % 90) != 0)
        return {};

    PdfPage result;
    result.index = pageIndex;
    result.bounds = { static_cast<float> (mediaBox.origin.x),
                      static_cast<float> (mediaBox.origin.y),
                      static_cast<float> (mediaBox.size.width),
                      static_cast<float> (mediaBox.size.height) };
    result.rotation = rotation;
    return result;
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
