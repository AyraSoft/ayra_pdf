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

#pragma once

#if JUCE_MAC || JUCE_IOS

namespace ayra
{

/** Implementazione CoreGraphics di PdfRenderer per macOS e iOS.
 *
 *  Usa CGPDFDocument nativo — zero dipendenze esterne, nessuna libreria
 *  di terze parti richiesta. Rendering ad alta qualita' con antialiasing
 *  CoreGraphics e supporto nativo per font embedding, trasparenza, ecc.
 *
 *  Migrazione da MacPDFComponent.mm — completata nella Fase 2 del
 *  refactoring di ayra_pdf.
 *
 *  Usa il pattern Pimpl per isolare i tipi ObjC/CoreGraphics dagli
 *  header C++ — necessario per evitare contaminazione degli altri moduli.
 *
 *  @see PdfRenderer, PdfDocument
 */
class MacPdfRenderer final : public PdfRenderer
{
public:
    MacPdfRenderer();
    ~MacPdfRenderer() override;

    [[nodiscard]] bool loadFromFile   (const juce::File& file)             noexcept override;
    [[nodiscard]] bool loadFromMemory (const void* data, size_t sizeBytes) noexcept override;
    [[nodiscard]] bool saveToFile     (const juce::File& destFile)   const noexcept override;
    [[nodiscard]] bool saveToMemory   (juce::MemoryBlock& destData)  const noexcept override;
    void               close          ()                                   noexcept override;

    [[nodiscard]] bool                         isLoaded    ()              const noexcept override;
    [[nodiscard]] int                          getPageCount()              const noexcept override;
    [[nodiscard]] PdfPage                      getPage     (int pageIndex) const noexcept override;
    [[nodiscard]] juce::Image                  renderPage  (int pageIndex, float scale) noexcept override;
    [[nodiscard]] juce::Array<PdfSearchResult> findText    (const juce::String& query, int pageIndex) noexcept override;
    [[nodiscard]] juce::String                 extractText (int pageIndex) noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;  ///< Pimpl: isola CGPDFDocumentRef e ObjC dagli header C++

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacPdfRenderer)
};

} // namespace ayra

#endif // JUCE_MAC || JUCE_IOS
