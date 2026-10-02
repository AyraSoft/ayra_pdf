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

//==============================================================================
// Default LookAndFeel

PdfDefaultLookAndFeel& PdfDefaultLookAndFeel::getDefaultInstance()
{
    static PdfDefaultLookAndFeel instance;
    return instance;
}

void PdfDefaultLookAndFeel::drawPdfViewBackground (juce::Graphics& g,
                                                    int width,
                                                    int height,
                                                    PdfViewComponent& comp)
{
    g.setColour (resolveColour (comp,
                                PdfViewComponent::backgroundColourId,
                                juce::Colour (0xff2a2a2a)));
    g.fillRect (0, 0, width, height);
}

void PdfDefaultLookAndFeel::drawPdfViewNoDocument (juce::Graphics& g,
                                                    int width,
                                                    int height,
                                                    PdfViewComponent& comp)
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

void PdfDefaultLookAndFeel::drawPdfViewPageBackground (juce::Graphics& g,
                                                        juce::Rectangle<float> pageBounds,
                                                        PdfViewComponent& comp)
{
    g.setColour (resolveColour (comp,
                                PdfViewComponent::pageColourId,
                                juce::Colours::white));
    g.fillRect (pageBounds);
}

void PdfDefaultLookAndFeel::drawPdfViewPageShadow (juce::Graphics& g,
                                                    juce::Rectangle<float> pageBounds,
                                                    PdfViewComponent& comp)
{
    const auto shadow = pageBounds.expanded (4.0f).translated (3.0f, 3.0f);
    g.setColour (resolveColour (comp,
                                PdfViewComponent::shadowColourId,
                                juce::Colours::black.withAlpha (0.4f)));
    g.fillRect (shadow);
}

juce::Colour PdfDefaultLookAndFeel::resolveColour (PdfViewComponent& comp,
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

//==============================================================================
// LookAndFeel forwarding

void PdfViewComponent::LookAndFeelMethods::drawPdfViewBackground (juce::Graphics& g,
                                                                  int width,
                                                                  int height,
                                                                  PdfViewComponent& comp)
{
    PdfDefaultLookAndFeel::getDefaultInstance().drawPdfViewBackground (g, width, height, comp);
}

void PdfViewComponent::LookAndFeelMethods::drawPdfViewNoDocument (juce::Graphics& g,
                                                                  int width,
                                                                  int height,
                                                                  PdfViewComponent& comp)
{
    PdfDefaultLookAndFeel::getDefaultInstance().drawPdfViewNoDocument (g, width, height, comp);
}

void PdfViewComponent::LookAndFeelMethods::drawPdfViewPageBackground (juce::Graphics& g,
                                                                      juce::Rectangle<float> pageBounds,
                                                                      PdfViewComponent& comp)
{
    PdfDefaultLookAndFeel::getDefaultInstance().drawPdfViewPageBackground (g, pageBounds, comp);
}

void PdfViewComponent::LookAndFeelMethods::drawPdfViewPageShadow (juce::Graphics& g,
                                                                  juce::Rectangle<float> pageBounds,
                                                                  PdfViewComponent& comp)
{
    PdfDefaultLookAndFeel::getDefaultInstance().drawPdfViewPageShadow (g, pageBounds, comp);
}

//==============================================================================

PdfViewComponent::PdfViewComponent() = default;
PdfViewComponent::~PdfViewComponent() = default;

void PdfViewComponent::loadDocument (const juce::String& filePath)
{
    auto candidate = std::make_unique<PdfDocument>();

    if (!candidate->open (juce::File (filePath)) || candidate->getPageCount() <= 0)
        return;

    ownedDocument = std::move (candidate);
    activateDocument (*ownedDocument);
}

bool PdfViewComponent::thereIsADocumentLoaded() const
{
    return currentDocument != nullptr
        && currentPage > 0
        && currentDocument->isOpen();
}

int PdfViewComponent::getTotPagesNum() const
{
    return thereIsADocumentLoaded() ? currentDocument->getPageCount() : 0;
}

int PdfViewComponent::getCurrentPageOnScreen() const
{
    return thereIsADocumentLoaded() ? currentPage : 0;
}

void PdfViewComponent::setPageNumber (int pageNumber)
{
    if (!thereIsADocumentLoaded())
        return;

    const int totalPages = currentDocument->getPageCount();

    if (pageNumber < 1 || pageNumber > totalPages || pageNumber == currentPage)
        return;

    currentPage = pageNumber;
    topLeft = {};
    invalidatePageCache();
    notifyPageChanged();
}

float PdfViewComponent::getDocumentWidth() const
{
    const auto page = getCurrentPageInfo();

    if (!page.isValid())
        return 0.0f;

    return page.getDisplaySize().x;
}

float PdfViewComponent::getDocumentHeight() const
{
    const auto page = getCurrentPageInfo();

    if (!page.isValid())
        return 0.0f;

    return page.getDisplaySize().y;
}

float PdfViewComponent::getCurrentPageZoom() const
{
    return currentZoom;
}

void PdfViewComponent::setCurrentPageZoom (float zoom, juce::Point<float> handlePoint)
{
    juce::ignoreUnused (zoom, handlePoint);
    jassertfalse; // Viewport zoom/anchor belongs to the interaction layer.
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
    const auto page = getCurrentPageInfo();

    if (!page.isValid() || !std::isfinite (currentZoom) || currentZoom <= 0.0f)
        return {};

    const auto displaySize = page.getDisplaySize();

    return { topLeft.x,
             topLeft.y,
             displaySize.x * currentZoom,
             displaySize.y * currentZoom };
}

void PdfViewComponent::exportCurrentDocument (const juce::String& withName,
                                              const juce::String& folderPath) const
{
    if (!thereIsADocumentLoaded())
        return;

    const auto legalName = juce::File::createLegalFileName (withName.trim());

    if (legalName.isEmpty() || legalName == "." || legalName == "..")
        return;

    const juce::File folder (folderPath);

    if (!folder.isDirectory())
        return;

    const auto destination = folder.getChildFile (legalName).withFileExtension (".pdf");
    (void) currentDocument->save (destination);
}

void PdfViewComponent::loadDocumentFromMemoryBlock (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    auto candidate = std::make_unique<PdfDocument>();

    if (!candidate->open (data, static_cast<size_t> (sizeInBytes))
        || candidate->getPageCount() <= 0)
    {
        return;
    }

    ownedDocument = std::move (candidate);
    activateDocument (*ownedDocument);
}

void PdfViewComponent::getMemoryBlockFromDocument (juce::MemoryBlock& destData)
{
    if (thereIsADocumentLoaded())
        (void) currentDocument->saveToMemoryBlock (destData);
}

void PdfViewComponent::setDocument (PdfDocument& doc)
{
    if (&doc == currentDocument)
        return;

    if (!doc.isOpen() || doc.getPageCount() <= 0)
        return;

    ownedDocument.reset();
    activateDocument (doc);
}

void PdfViewComponent::addListener (Listener* l)
{
    listeners.add (l);
}

void PdfViewComponent::removeListener (Listener* l)
{
    listeners.remove (l);
}

void PdfViewComponent::paint (juce::Graphics& g)
{
    auto& laf = getLAF();
    laf.drawPdfViewBackground (g, getWidth(), getHeight(), *this);

    if (!thereIsADocumentLoaded())
    {
        laf.drawPdfViewNoDocument (g, getWidth(), getHeight(), *this);
        return;
    }

    const auto pageBounds = getCurrentPageBounds();

    if (pageBounds.isEmpty())
    {
        laf.drawPdfViewNoDocument (g, getWidth(), getHeight(), *this);
        return;
    }

    laf.drawPdfViewPageShadow (g, pageBounds, *this);
    laf.drawPdfViewPageBackground (g, pageBounds, *this);

    if (!pageCacheReady)
    {
        cachedPageImage = currentDocument->renderPage (currentPage - 1, currentZoom);
        pageCacheReady = true;
    }

    if (cachedPageImage.isValid())
        g.drawImage (cachedPageImage, pageBounds);
}

void PdfViewComponent::resized()
{
    repaint();
}

PdfViewComponent::LookAndFeelMethods& PdfViewComponent::getLAF()
{
    if (auto* l = dynamic_cast<LookAndFeelMethods*> (&getLookAndFeel()))
        return *l;

    return PdfDefaultLookAndFeel::getDefaultInstance();
}

PdfPage PdfViewComponent::getCurrentPageInfo() const
{
    if (!thereIsADocumentLoaded())
        return {};

    return currentDocument->getPage (currentPage - 1);
}

void PdfViewComponent::activateDocument (PdfDocument& doc)
{
    currentDocument = &doc;
    currentPage = 1;
    currentZoom = 1.0f;
    topLeft = {};
    invalidatePageCache();
    notifyDocumentLoaded();
}

void PdfViewComponent::invalidatePageCache()
{
    cachedPageImage = {};
    pageCacheReady = false;
    repaint();
}

void PdfViewComponent::notifyDocumentLoaded()
{
    juce::Component::BailOutChecker checker (this);
    listeners.callChecked (checker, [this] (Listener& listener)
    {
        listener.pdfDocumentLoaded (this);
    });

    if (checker.shouldBailOut())
        return;

    if (onDocumentLoaded)
        onDocumentLoaded();
}

void PdfViewComponent::notifyPageChanged()
{
    const int page = currentPage;
    juce::Component::BailOutChecker checker (this);

    listeners.callChecked (checker, [this, page] (Listener& listener)
    {
        listener.pdfPageChanged (this, page);
    });

    if (checker.shouldBailOut())
        return;

    if (onPageChanged)
        onPageChanged (page);
}

} // namespace ayra
