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

/** Interfaccia LookAndFeel aggregata per il modulo ayra_pdf.
 *
 *  L'applicazione finale eredita questa struttura insieme a
 *  juce::LookAndFeel_V4 per customizzare il rendering dei widget PDF.
 *
 *  Segue il pattern multi-modulo Ayra: questa interfaccia NON eredita
 *  da juce::LookAndFeel_V4 per evitare diamond inheritance quando
 *  l'app combina i LookAndFeel di piu' moduli.
 *
 *  ### Esempio di integrazione nell'app
 *  @code
 *  class MyAppLookAndFeel : public juce::LookAndFeel_V4,
 *                           public ayra::PdfLookAndFeelMethods,
 *                           public ayra::TimelineLookAndFeelMethods
 *  {
 *      // Override solo i metodi da personalizzare
 *  };
 *  @endcode
 *
 *  @see PdfViewComponent::LookAndFeelMethods, PdfDefaultLookAndFeel
 */
struct PdfLookAndFeelMethods : public PdfViewComponent::LookAndFeelMethods
{
    // Eredita tutti i metodi virtual da PdfViewComponent::LookAndFeelMethods.
    // Aggiungere qui i metodi di eventuali widget aggiuntivi del modulo (future estensioni).
};

} // namespace ayra
