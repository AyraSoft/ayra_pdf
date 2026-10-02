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

// PdfDocument e' esclusivamente factory + facade.
// Tutta la semantica PDF vive nei renderer concreti.

namespace ayra
{

PdfDocument::PdfDocument()
{
#if JUCE_MAC || JUCE_IOS
    renderer = std::make_unique<MacPdfRenderer>();
#elif JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID
    renderer = std::make_unique<PdfiumRenderer>();
#else
    #error "ayra_pdf: piattaforma non supportata; aggiungere un PdfRenderer canonico."
#endif
}

PdfDocument::~PdfDocument() = default;

bool PdfDocument::open (const juce::File& file) noexcept
{
    return renderer != nullptr && renderer->loadFromFile (file);
}

bool PdfDocument::open (const void* data, size_t sizeBytes) noexcept
{
    return renderer != nullptr && renderer->loadFromMemory (data, sizeBytes);
}

bool PdfDocument::save (const juce::File& destFile) const noexcept
{
    return renderer != nullptr && renderer->saveToFile (destFile);
}

bool PdfDocument::saveToMemoryBlock (juce::MemoryBlock& destData) const noexcept
{
    return renderer != nullptr && renderer->saveToMemory (destData);
}

bool PdfDocument::sourceBytesEqualFile (const juce::File& file) const noexcept
{
    return renderer != nullptr && renderer->sourceBytesEqualFile (file);
}

void PdfDocument::close() noexcept
{
    if (renderer != nullptr)
        renderer->close();
}

std::uint64_t PdfDocument::getMaximumDocumentBytes() noexcept
{
    return detail::maxDocumentBytes;
}

bool PdfDocument::isOpen() const noexcept
{
    return renderer != nullptr && renderer->isLoaded();
}

int PdfDocument::getPageCount() const noexcept
{
    return renderer != nullptr ? renderer->getPageCount() : 0;
}

PdfPage PdfDocument::getPage (int pageIndex) const noexcept
{
    return renderer != nullptr ? renderer->getPage (pageIndex) : PdfPage {};
}

juce::Image PdfDocument::renderPage (int pageIndex, float scale) noexcept
{
    return renderer != nullptr ? renderer->renderPage (pageIndex, scale) : juce::Image {};
}

juce::Array<PdfSearchResult> PdfDocument::findText (const juce::String& query, int pageIndex) noexcept
{
    return renderer != nullptr ? renderer->findText (query, pageIndex)
                               : juce::Array<PdfSearchResult> {};
}

juce::String PdfDocument::extractText (int pageIndex) noexcept
{
    return renderer != nullptr ? renderer->extractText (pageIndex) : juce::String {};
}

PdfRenderer* PdfDocument::getRenderer() noexcept
{
    return renderer.get();
}

} // namespace ayra
