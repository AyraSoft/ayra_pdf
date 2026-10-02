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

namespace ayra
{

juce::Rectangle<float> PdfPage::getDisplayBounds (juce::Rectangle<float> pageSpaceBounds) const noexcept
{
    if (!isValid() || pageSpaceBounds.isEmpty())
        return {};

    const auto clipped = pageSpaceBounds.getIntersection (bounds);

    if (clipped.isEmpty())
        return {};

    const float localX = clipped.getX() - bounds.getX();
    const float localBottom = clipped.getY() - bounds.getY();
    const float width = clipped.getWidth();
    const float height = clipped.getHeight();
    const float pageWidth = bounds.getWidth();
    const float pageHeight = bounds.getHeight();

    switch (rotation)
    {
        case 0:
            return { localX,
                     pageHeight - (localBottom + height),
                     width,
                     height };

        case 90:
            return { localBottom,
                     localX,
                     height,
                     width };

        case 180:
            return { pageWidth - (localX + width),
                     localBottom,
                     width,
                     height };

        case 270:
            return { pageHeight - (localBottom + height),
                     pageWidth - (localX + width),
                     height,
                     width };

        default:
            return {};
    }
}

} // namespace ayra
