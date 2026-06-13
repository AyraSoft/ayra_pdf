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

// Implementazione PdfViewComponent — skeleton
// Fase 5: implementazione completa (dopo MacPdfRenderer Fase 2 e PdfiumRenderer Fase 3)

namespace ayra
{

PdfViewComponent::PdfViewComponent()
{
    // TODO: Fase 5 — setup iniziale
}

PdfViewComponent::~PdfViewComponent() = default;

void PdfViewComponent::loadDocument (const juce::String& filePath)
{
    jassertfalse; // TODO: Fase 5 — ownedDocument.open(File(filePath)); currentDocument = &ownedDocument;
}

bool PdfViewComponent::thereIsADocumentLoaded() const
{
    // TODO: Fase 5
    return false;
}

int PdfViewComponent::getTotPagesNum() const
{
    // TODO: Fase 5
    return 0;
}

int PdfViewComponent::getCurrentPageOnScreen() const
{
    return currentPage;
}

void PdfViewComponent::setPageNumber (int pageNumber)
{
    jassertfalse; // TODO: Fase 5
}

float PdfViewComponent::getDocumentWidth() const
{
    // TODO: Fase 5
    return 0.0f;
}

float PdfViewComponent::getDocumentHeight() const
{
    // TODO: Fase 5
    return 0.0f;
}

float PdfViewComponent::getCurrentPageZoom() const
{
    return currentZoom;
}

void PdfViewComponent::setCurrentPageZoom (float zoom, juce::Point<float> handlePoint)
{
    jassertfalse; // TODO: Fase 5
}

juce::Point<float> PdfViewComponent::getCurrentPageTopLeftPosition() const
{
    return topLeft;
}

void PdfViewComponent::setCurrentPageTopLeftPosition (juce::Point<float> newPos)
{
    topLeft = newPos;
    repaint();
}

juce::Rectangle<float> PdfViewComponent::getCurrentPageBounds() const
{
    // TODO: Fase 5
    return {};
}

void PdfViewComponent::exportCurrentDocument (const juce::String& withName, const juce::String& folderPath) const
{
    jassertfalse; // TODO: Fase 5 — currentDocument->save(File(folderPath).getChildFile(withName + ".pdf"))
}

void PdfViewComponent::loadDocumentFromMemoryBlock (const void* data, int sizeInBytes)
{
    jassertfalse; // TODO: Fase 5 — ownedDocument.open(data, size); currentDocument = &ownedDocument;
}

void PdfViewComponent::getMemoryBlockFromDocument (juce::MemoryBlock& destData)
{
    jassertfalse; // TODO: Fase 5 — currentDocument->saveToMemoryBlock(destData)
}

void PdfViewComponent::setDocument (PdfDocument& doc)
{
    currentDocument = &doc;
    currentPage     = 1;
    currentZoom     = 1.0f;
    topLeft         = {};
    repaint();
    listeners.call ([this] (Listener& l) { l.pdfDocumentLoaded (this); });
    if (onDocumentLoaded) { onDocumentLoaded(); }
}

void PdfViewComponent::addListener (Listener* l)    { listeners.add (l); }
void PdfViewComponent::removeListener (Listener* l) { listeners.remove (l); }

void PdfViewComponent::paint (juce::Graphics& g)
{
    auto& laf = getLAF();
    laf.drawPdfViewBackground (g, getWidth(), getHeight(), *this);

    if (currentDocument == nullptr || !currentDocument->isOpen())
    {
        laf.drawPdfViewNoDocument (g, getWidth(), getHeight(), *this);
        return;
    }

    // TODO: Fase 5 — renderizzare la pagina corrente con currentDocument->renderPage()
    // Per ora placeholder
    const auto pageBounds = getCurrentPageBounds();
    if (!pageBounds.isEmpty())
    {
        laf.drawPdfViewPageShadow (g, pageBounds, *this);
        g.setColour (findColour (pageColourId));
        g.fillRect (pageBounds);
    }
}

void PdfViewComponent::resized()
{
    // TODO: Fase 5 — ricalcolare la posizione della pagina in base al nuovo bounds
}

PdfViewComponent::LookAndFeelMethods& PdfViewComponent::getLAF()
{
    if (auto* l = dynamic_cast<LookAndFeelMethods*> (&getLookAndFeel()))
        return *l;
    return PdfDefaultLookAndFeel::getDefaultInstance();
}

} // namespace ayra
