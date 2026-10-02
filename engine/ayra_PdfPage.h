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

namespace ayra
{

/** Informazioni geometriche e di metadati di una singola pagina PDF.
 *
 *  Value object immutabile dopo la costruzione. Contiene le coordinate
 *  nel visible page box (MediaBox intersecato con CropBox), in PDF page/user space:
 *
 *  Usato come output di PdfDocument::getPage() e PdfRenderer::getPage().
 *
 *  @see PdfDocument::getPage()
 */
struct PdfPage
{
    int                    index    {};  ///< Indice 0-based della pagina nel documento
    juce::Rectangle<float> bounds   {};  ///< Visible box: MediaBox intersect CropBox, 72 dpi, lower-left/Y-up
    int                    rotation {};  ///< Rotazione in gradi: 0, 90, 180 o 270

    /** Ritorna true se la pagina ha un indice valido e dimensioni non zero. */
    [[nodiscard]] inline bool isValid() const noexcept { return index >= 0 && !bounds.isEmpty(); }

    /** Dimensione visuale della pagina dopo l'applicazione della rotazione.
     *  Il punto usa x = larghezza e y = altezza, sempre in punti PDF (1/72").
     */
    [[nodiscard]] inline juce::Point<float> getDisplaySize() const noexcept
    {
        const bool swapsAxes = rotation == 90 || rotation == 270;
        return swapsAxes ? juce::Point<float> { bounds.getHeight(), bounds.getWidth() }
                         : juce::Point<float> { bounds.getWidth(), bounds.getHeight() };
    }

    /** Converte bounds dal PDF page space (lower-left/Y-up) al display page space
     *  logico (top-left/Y-down), applicando /Rotate ma non zoom o viewport offset.
     *  L'input viene prima clippato al visible page box canonico.
     */
    [[nodiscard]] juce::Rectangle<float> getDisplayBounds (juce::Rectangle<float> pageSpaceBounds) const noexcept;
};

} // namespace ayra
