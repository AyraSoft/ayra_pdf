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

#if JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID

namespace ayra
{

/** Implementazione PDFium (Apache 2.0) di PdfRenderer per Windows, Linux e Android.
 *
 *  PDFium e' il renderer PDF di Chrome/Chromium — alta compatibilita' con lo
 *  standard PDF, supporto per form interattivi, JavaScript, annotazioni.
 *
 *  Richiede i binari precompilati in:
 *    third_party/pdfium/<platform>/  (scarica con scripts/setup_pdfium.sh)
 *  e gli header in:
 *    third_party/pdfium/include/
 *
 *  Fase 3: implementazione completa.
 *
 *  Usa il pattern Pimpl per isolare i tipi PDFium (FPDF_DOCUMENT, FPDF_PAGE)
 *  dagli header C++ — necessario per evitare contaminazione degli altri moduli
 *  e per rendere l'header compilabile anche senza PDFium installato.
 *
 *  @see PdfRenderer, PdfDocument, MacPdfRenderer
 */
class PdfiumRenderer final : public PdfRenderer
{
public:
    PdfiumRenderer();
    ~PdfiumRenderer() override;

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
    std::unique_ptr<Impl> impl;  ///< Pimpl: isola FPDF_DOCUMENT e tipi PDFium dagli header C++

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PdfiumRenderer)
};

} // namespace ayra

#endif // JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID
