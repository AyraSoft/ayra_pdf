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
    static PdfDefaultLookAndFeel& getDefaultInstance();

    void drawPdfViewBackground (juce::Graphics&,
                                int width,
                                int height,
                                PdfViewComponent&) override;

    void drawPdfViewNoDocument (juce::Graphics&,
                                int width,
                                int height,
                                PdfViewComponent&) override;

    void drawPdfViewPageBackground (juce::Graphics&,
                                    juce::Rectangle<float> pageBounds,
                                    PdfViewComponent&) override;

    void drawPdfViewPageShadow (juce::Graphics&,
                                juce::Rectangle<float> pageBounds,
                                PdfViewComponent&) override;

    void drawPdfViewSearchHighlight (juce::Graphics&,
                                     juce::Rectangle<float> highlightBounds,
                                     PdfViewComponent&) override;

private:
    PdfDefaultLookAndFeel() = default;

    static juce::Colour resolveColour (PdfViewComponent&,
                                       int colourId,
                                       juce::Colour fallback);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PdfDefaultLookAndFeel)
};

} // namespace ayra
