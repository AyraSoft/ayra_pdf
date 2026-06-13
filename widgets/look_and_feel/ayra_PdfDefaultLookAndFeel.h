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

/** Implementazione default del LookAndFeel per il modulo ayra_pdf.
 *
 *  Usata come fallback da PdfViewComponent::getLAF() quando il LookAndFeel
 *  corrente dell'applicazione non implementa PdfLookAndFeelMethods.
 *  In questo modo il widget funziona out-of-the-box senza alcun setup.
 *
 *  Singleton leggero — NON eredita da juce::LookAndFeel_V4 per non
 *  interferire con il LookAndFeel dell'app.
 *
 *  @see PdfLookAndFeelMethods, PdfViewComponent::getLAF()
 */
class PdfDefaultLookAndFeel final : public PdfLookAndFeelMethods
{
public:
    /** Ritorna l'istanza singleton del default LookAndFeel. */
    static PdfDefaultLookAndFeel& getDefaultInstance()
    {
        static PdfDefaultLookAndFeel instance;
        return instance;
    }

    //==============================================================================

    void drawPdfViewBackground (juce::Graphics& g, int width, int height, PdfViewComponent&) override
    {
        g.fillAll (juce::Colour (0xff2a2a2a));
    }

    void drawPdfViewNoDocument (juce::Graphics& g, int width, int height, PdfViewComponent&) override
    {
        g.setColour (juce::Colours::grey);
        g.setFont (14.0f);
        g.drawText ("Nessun documento caricato", 0, 0, width, height, juce::Justification::centred);
    }

    void drawPdfViewPageShadow (juce::Graphics& g, juce::Rectangle<float> pageBounds, PdfViewComponent&) override
    {
        const auto shadow = pageBounds.expanded (4.0f).translated (3.0f, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRect (shadow);
    }

private:
    PdfDefaultLookAndFeel() = default;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PdfDefaultLookAndFeel)
};

} // namespace ayra
