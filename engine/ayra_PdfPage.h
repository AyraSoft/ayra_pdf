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
 *  in PDF user space (punti tipografici: 1 punto = 1/72 pollice).
 *
 *  Usato come output di PdfDocument::getPage() e PdfRenderer::getPage().
 *
 *  @see PdfDocument::getPage()
 */
struct PdfPage
{
    int                    index    {};  ///< Indice 0-based della pagina nel documento
    juce::Rectangle<float> bounds   {};  ///< Dimensioni in PDF user space (punti tipografici)
    int                    rotation {};  ///< Rotazione in gradi: 0, 90, 180 o 270

    /** Ritorna true se la pagina ha un indice valido e dimensioni non zero. */
    [[nodiscard]] inline bool isValid() const noexcept { return index >= 0 && !bounds.isEmpty(); }
};

} // namespace ayra
