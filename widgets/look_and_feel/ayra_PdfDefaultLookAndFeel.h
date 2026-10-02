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

/** Default visual stateless per i widget ayra_pdf.
 *  L'app puo' sostituire qualsiasi elemento ereditando PdfLookAndFeelMethods.
 */
class PdfDefaultLookAndFeel final : public PdfLookAndFeelMethods
{
public:
    static PdfDefaultLookAndFeel& getDefaultInstance()
    {
        static PdfDefaultLookAndFeel instance;
        return instance;
    }

    void drawPdfViewBackground (juce::Graphics& g,
                                int width,
                                int height,
                                PdfViewComponent& comp) override
    {
        g.setColour (resolveColour (comp,
                                    PdfViewComponent::backgroundColourId,
                                    juce::Colour (0xff2a2a2a)));
        g.fillRect (0, 0, width, height);
    }

    void drawPdfViewNoDocument (juce::Graphics& g,
                                int width,
                                int height,
                                PdfViewComponent& comp) override
    {
        g.setColour (resolveColour (comp,
                                    PdfViewComponent::noDocumentTextColourId,
                                    juce::Colours::grey));
        g.setFont (14.0f);
        g.drawText ("Nessun documento caricato",
                    0,
                    0,
                    width,
                    height,
                    juce::Justification::centred);
    }

    void drawPdfViewPageBackground (juce::Graphics& g,
                                    juce::Rectangle<float> pageBounds,
                                    PdfViewComponent& comp) override
    {
        g.setColour (resolveColour (comp,
                                    PdfViewComponent::pageColourId,
                                    juce::Colours::white));
        g.fillRect (pageBounds);
    }

    void drawPdfViewPageShadow (juce::Graphics& g,
                                juce::Rectangle<float> pageBounds,
                                PdfViewComponent& comp) override
    {
        const auto shadow = pageBounds.expanded (4.0f).translated (3.0f, 3.0f);
        g.setColour (resolveColour (comp,
                                    PdfViewComponent::shadowColourId,
                                    juce::Colours::black.withAlpha (0.4f)));
        g.fillRect (shadow);
    }

private:
    PdfDefaultLookAndFeel() = default;

    static juce::Colour resolveColour (PdfViewComponent& comp,
                                       int colourId,
                                       juce::Colour fallback)
    {
        if (comp.isColourSpecified (colourId)
            || comp.getLookAndFeel().isColourSpecified (colourId))
        {
            return comp.findColour (colourId);
        }

        return fallback;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PdfDefaultLookAndFeel)
};

} // namespace ayra
