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

// Implementazione PdfDocument — skeleton
// Fase 2: MacPdfRenderer (CoreGraphics) — migrazione da MacPDFComponent.mm
// Fase 3: PdfiumRenderer (PDFium Apache 2.0) — Win/Linux/Android
// Fase 4: collegamento automatico renderer per piattaforma in PdfDocument()

namespace ayra
{

PdfDocument::PdfDocument()
{
    // TODO: Fase 4 — istanziare il renderer corretto per la piattaforma
    // #if JUCE_MAC || JUCE_IOS
    //     renderer = std::make_unique<MacPdfRenderer>();
    // #else
    //     renderer = std::make_unique<PdfiumRenderer>();
    // #endif
}

PdfDocument::~PdfDocument() = default;

bool PdfDocument::open (const juce::File& file) noexcept
{
    jassertfalse; // TODO: Fase 4
    return false;
}

bool PdfDocument::open (const void* data, size_t sizeBytes) noexcept
{
    jassertfalse; // TODO: Fase 4
    return false;
}

bool PdfDocument::save (const juce::File& destFile) const noexcept
{
    jassertfalse; // TODO: Fase 4
    return false;
}

bool PdfDocument::saveToMemoryBlock (juce::MemoryBlock& destData) const noexcept
{
    jassertfalse; // TODO: Fase 4
    return false;
}

void PdfDocument::close() noexcept
{
    // TODO: Fase 4 — renderer->close()
}

bool PdfDocument::isOpen() const noexcept
{
    // TODO: Fase 4
    return false;
}

int PdfDocument::getPageCount() const noexcept
{
    // TODO: Fase 4
    return 0;
}

PdfPage PdfDocument::getPage (int pageIndex) const noexcept
{
    jassertfalse; // TODO: Fase 4
    return {};
}

juce::Image PdfDocument::renderPage (int pageIndex, float scale) noexcept
{
    jassertfalse; // TODO: Fase 4
    return {};
}

juce::Array<PdfSearchResult> PdfDocument::findText (const juce::String& query, int pageIndex) noexcept
{
    jassertfalse; // TODO: Fase 4
    return {};
}

juce::String PdfDocument::extractText (int pageIndex) noexcept
{
    jassertfalse; // TODO: Fase 4
    return {};
}

PdfRenderer* PdfDocument::getRenderer() noexcept
{
    return renderer.get();
}

} // namespace ayra
