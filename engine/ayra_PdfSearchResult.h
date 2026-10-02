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

/** Risultato di una ricerca testuale in un documento PDF.
 *
 *  Restituito da PdfDocument::findText() come elemento dell'array di risultati.
 *  Le coordinate sono in PDF page/user space: 72 dpi, origine lower-left,
 *  asse Y verso l'alto. La conversione verso coordinate widget/schermo appartiene al consumer.
 *
 *  @see PdfDocument::findText()
 */
struct PdfSearchResult
{
    int                    pageIndex {};  ///< Indice 0-based della pagina contenente il risultato
    juce::Rectangle<float> bounds    {};  ///< Bounding box in PDF page space, 72 dpi, lower-left/Y-up
    juce::String           text      {};  ///< Testo trovato, eventualmente con snippet di contesto

    /** Ritorna true se il risultato punta a una pagina valida con bounds non zero. */
    [[nodiscard]] inline bool isValid() const noexcept { return pageIndex >= 0 && !bounds.isEmpty(); }
};

} // namespace ayra
