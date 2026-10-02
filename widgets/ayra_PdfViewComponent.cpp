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

namespace
{

[[nodiscard]] juce::ThreadPool& getPdfRenderPool()
{
    static juce::ThreadPool pool { 1 };
    return pool;
}

} // namespace

struct PdfViewComponent::RenderState
{
    std::atomic<std::uint64_t> generation { 0 };
};

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

PdfViewComponent::PdfViewComponent()
    : renderState (std::make_shared<RenderState>())
{
}

PdfViewComponent::~PdfViewComponent()
{
    if (renderState != nullptr)
        renderState->generation.fetch_add (1, std::memory_order_release);

    currentDocument.reset();
}

void PdfViewComponent::loadDocument (const juce::String& filePath)
{
    auto candidate = std::make_shared<PdfDocument>();

    if (!candidate->open (juce::File (filePath)) || candidate->getPageCount() <= 0)
        return;

    activateDocument (std::move (candidate));
}

bool PdfViewComponent::thereIsADocumentLoaded() const
{
    return currentDocument != nullptr
        && currentPageCount > 0
        && currentPage >= 1
        && currentPage <= currentPageCount;
}

int PdfViewComponent::getTotPagesNum() const
{
    return thereIsADocumentLoaded() ? currentPageCount : 0;
}

int PdfViewComponent::getCurrentPageOnScreen() const
{
    return thereIsADocumentLoaded() ? currentPage : 0;
}

void PdfViewComponent::setPageNumber (int pageNumber)
{
    if (!thereIsADocumentLoaded())
        return;

    if (pageNumber < 1 || pageNumber > currentPageCount || pageNumber == currentPage)
        return;

    currentPage = pageNumber;
    topLeft = {};
    cachedPageInfo = {};
    invalidatePageCache();
    requestPageRender();
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

    auto candidate = std::make_shared<PdfDocument>();

    if (!candidate->open (data, static_cast<size_t> (sizeInBytes))
        || candidate->getPageCount() <= 0)
    {
        return;
    }

    activateDocument (std::move (candidate));
}

void PdfViewComponent::getMemoryBlockFromDocument (juce::MemoryBlock& destData)
{
    if (thereIsADocumentLoaded())
        (void) currentDocument->saveToMemoryBlock (destData);
}

void PdfViewComponent::setDocument (std::shared_ptr<PdfDocument> doc)
{
    if (doc == nullptr || doc == currentDocument)
        return;

    if (!doc->isOpen() || doc->getPageCount() <= 0)
        return;

    activateDocument (std::move (doc));
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
        return;

    laf.drawPdfViewPageShadow (g, pageBounds, *this);
    laf.drawPdfViewPageBackground (g, pageBounds, *this);

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
    return thereIsADocumentLoaded() ? cachedPageInfo : PdfPage {};
}

void PdfViewComponent::activateDocument (std::shared_ptr<PdfDocument> doc)
{
    currentDocument = std::move (doc);
    currentPageCount = currentDocument != nullptr ? currentDocument->getPageCount() : 0;
    currentPage = currentPageCount > 0 ? 1 : 0;
    currentZoom = 1.0f;
    topLeft = {};
    cachedPageInfo = currentPage > 0 ? currentDocument->getPage (0) : PdfPage {};

    invalidatePageCache();
    requestPageRender();
    notifyDocumentLoaded();
}

void PdfViewComponent::invalidatePageCache()
{
    if (renderState != nullptr)
        renderState->generation.fetch_add (1, std::memory_order_release);

    cachedPageImage = {};
    repaint();
}

void PdfViewComponent::requestPageRender()
{
    if (!thereIsADocumentLoaded() || renderState == nullptr)
        return;

    const auto state = renderState;
    const auto generation = state->generation.load (std::memory_order_acquire);
    const int pageIndex = currentPage - 1;
    const float rasterScale = currentZoom;
    const auto document = currentDocument;
    const juce::Component::SafePointer<PdfViewComponent> safeThis (this);

    try
    {
        getPdfRenderPool().addJob (
            [safeThis, state, document, generation, pageIndex, rasterScale]
            {
                if (state->generation.load (std::memory_order_acquire) != generation)
                    return;

                const auto page = document->getPage (pageIndex);

                if (!page.isValid()
                    || state->generation.load (std::memory_order_acquire) != generation)
                {
                    return;
                }

                auto image = document->renderPage (pageIndex, rasterScale);

                if (state->generation.load (std::memory_order_acquire) != generation)
                    return;

                try
                {
                    (void) juce::MessageManager::callAsync (
                        [safeThis, state, generation, page, image = std::move (image)] () mutable
                        {
                            if (safeThis != nullptr
                                && state->generation.load (std::memory_order_acquire) == generation)
                            {
                                safeThis->publishPageRender (generation, page, std::move (image));
                            }
                        });
                }
                catch (const std::bad_alloc&)
                {
                }
            });
    }
    catch (const std::bad_alloc&)
    {
    }
}

void PdfViewComponent::publishPageRender (std::uint64_t generation,
                                          PdfPage page,
                                          juce::Image image)
{
    if (renderState == nullptr
        || generation != renderState->generation.load (std::memory_order_acquire)
        || page.index != currentPage - 1)
    {
        return;
    }

    cachedPageInfo = page;
    cachedPageImage = std::move (image);
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
