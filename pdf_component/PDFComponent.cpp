/*
     ______  __    __  ______   ______
    |      \|  \  |  \/      \ |      \
     \▓▓▓▓▓▓\ ▓▓  | ▓▓  ▓▓▓▓▓▓\ \▓▓▓▓▓▓\
     /      ▓▓ ▓▓  | ▓▓ ▓▓   \▓▓/      ▓▓   Copyright 2023
    |  ▓▓▓▓▓▓▓ ▓▓__/ ▓▓ ▓▓     |  ▓▓▓▓▓▓▓   Ayra Soft
     \▓▓    ▓▓\▓▓    ▓▓ ▓▓      \▓▓    ▓▓   www.ayra.live
      \▓▓▓▓▓▓▓_\▓▓▓▓▓▓▓\▓▓       \▓▓▓▓▓▓▓
             |  \__| ▓▓
              \▓▓    ▓▓
                \▓▓▓▓▓▓
    
 Ayra uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace ayra
{

PDFComponent::PDFComponent()
{
#if JUCE_IOS || JUCE_MAC
    pdfViewComponent.reset(new MacPDFViewComponent {});
#elif JUCE_WINDOWS
#elif JUCE_ANDROID || JUCE_LINUX
#endif
    
    addAndMakeVisible(pdfViewComponent.get());
}

PDFComponent::~PDFComponent()
{
}

//==============================================================================

void PDFComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::white);
    pdfViewComponent->paint(g);
}

void PDFComponent::resized()
{
    pdfViewComponent->setBounds(getLocalBounds());
}

//==============================================================================

void PDFComponent::loadDocument(const juce::String filePath)
{
    pdfViewComponent->loadDocument(filePath);
}

bool PDFComponent::thereIsADocumentLoaded() const
{
    return pdfViewComponent->thereIsADocumentLoaded();
}

void PDFComponent::setPageNumber(const int pageNumber)
{
    pdfViewComponent->setPageNumber(pageNumber);
}

int PDFComponent::getTotPagesNum() const
{
    return  pdfViewComponent->getTotPagesNum();
}

int PDFComponent::getCurrentPageOnScreen() const
{
    return pdfViewComponent->getCurrentPageOnScreen();
}

float PDFComponent::getDocumentWidth() const
{
    return pdfViewComponent->getDocumentWidth();
}

float PDFComponent::getDocumentHeight() const
{
    return pdfViewComponent->getDocumentHeight();
}

//float PDFComponent::getCurrentPageSizeRatio() const
//{
//    if 
//    return pdfViewComponent->getCurrentPageSizeRatio();
//}

float PDFComponent::getCurrentPageZoom() const
{
    return pdfViewComponent->getCurrentPageZoom();
}

void PDFComponent::setCurrentPageZoom(const float zoom, const juce::Point<float> handlePoint)
{
    pdfViewComponent->setCurrentPageZoom(zoom, handlePoint);
}

juce::Point<float> PDFComponent::getCurrentPageTopLeftPosition() const
{
    return pdfViewComponent->getCurrentPageTopLeftPosition();
}

void PDFComponent::setCurrentPageTopLeftPosition(const juce::Point<float> newPos)
{
    pdfViewComponent->setCurrentPageTopLeftPosition(newPos);
}

juce::Rectangle<float> PDFComponent::getCurrentPageBounds() const
{
    return pdfViewComponent->getCurrentPageBounds();
}

void PDFComponent::exportCurrentDocument(const juce::String withName, const juce::String containingFolderPath) const
{
    pdfViewComponent->exportCurrentDocument(withName, containingFolderPath);
}

void PDFComponent::loadDocumentFromMemoryBlock(const void* data, int sizeInBytes)
{
    pdfViewComponent->loadDocumentFromMemoryBlock(data, sizeInBytes);
}

void PDFComponent::getMemoryBlockFromDocument(juce::MemoryBlock& destData)
{
    pdfViewComponent->getMemoryBlockFromDocument(destData);
}

} // namespace ayra
