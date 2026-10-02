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

constexpr float minZoom = 0.1f;
constexpr float maxZoom = 10.0f;
constexpr float minVisibleFraction = 0.1f;

[[nodiscard]] bool isFinitePoint (juce::Point<float> point) noexcept
{
    return std::isfinite (point.x) && std::isfinite (point.y);
}

[[nodiscard]] float clampRasterScaleToSafetyBudget (const PdfPage& page,
                                                     float desiredScale) noexcept
{
    if (!page.isValid()
        || !std::isfinite (desiredScale)
        || desiredScale <= 0.0f)
    {
        return 1.0f;
    }

    const auto displaySize = page.getDisplaySize();
    const double width = static_cast<double> (displaySize.x);
    const double height = static_cast<double> (displaySize.y);

    if (!std::isfinite (width) || !std::isfinite (height)
        || width <= 0.0 || height <= 0.0)
    {
        return 1.0f;
    }

    const double desired = static_cast<double> (desiredScale);
    const double maxDimension = static_cast<double> (detail::maxRasterDimension);
    const double maxPixels = static_cast<double> (detail::maxRasterPixels);
    const double pageArea = width * height;

    if (!std::isfinite (pageArea) || pageArea <= 0.0)
        return 1.0f;

    const double maxByWidth = maxDimension / width;
    const double maxByHeight = maxDimension / height;
    const double maxByPixels = std::sqrt (maxPixels / pageArea);
    double safe = std::min ({ desired, maxByWidth, maxByHeight, maxByPixels });

    if (!std::isfinite (safe) || safe <= 0.0)
        return 1.0f;

    const auto fitsIntegerRasterBudget = [width, height] (double scale) noexcept
    {
        if (!std::isfinite (scale) || scale <= 0.0)
            return false;

        const double rasterWidth = std::ceil (width * scale);
        const double rasterHeight = std::ceil (height * scale);

        if (!std::isfinite (rasterWidth) || !std::isfinite (rasterHeight)
            || rasterWidth <= 0.0 || rasterHeight <= 0.0
            || rasterWidth > static_cast<double> (detail::maxRasterDimension)
            || rasterHeight > static_cast<double> (detail::maxRasterDimension))
        {
            return false;
        }

        const auto widthPixels = static_cast<std::uint64_t> (rasterWidth);
        const auto heightPixels = static_cast<std::uint64_t> (rasterHeight);

        return widthPixels * heightPixels <= detail::maxRasterPixels;
    };

    // Renderer dimensions use ceil(). Validate the integer raster that will
    // actually be requested, not only the continuous theoretical scale.
    for (int attempt = 0;
         attempt < 8 && !fitsIntegerRasterBudget (safe);
         ++attempt)
    {
        safe *= 0.99;
    }

    if (!fitsIntegerRasterBudget (safe))
        return static_cast<float> (std::max (
            safe * 0.5,
            static_cast<double> (std::numeric_limits<float>::min())));

    return static_cast<float> (std::max (
        safe,
        static_cast<double> (std::numeric_limits<float>::min())));
}

[[nodiscard]] juce::ThreadPool& getPdfRenderPool()
{
    static juce::ThreadPool pool { 1 };
    return pool;
}

} // namespace

struct PdfViewComponent::RenderState final : private juce::AsyncUpdater,
                                             public std::enable_shared_from_this<RenderState>
{
    std::atomic<std::uint64_t> renderGeneration { 0 };
    std::atomic<std::uint64_t> searchGeneration { 0 };

    // Accesso esclusivo Message Thread: il worker non legge mai questo puntatore.
    PdfViewComponent* owner { nullptr };

    juce::CriticalSection requestLock;

    bool renderJobScheduled { false };
    bool renderRequestPending { false };
    std::shared_ptr<PdfDocument> renderDocument;
    std::uint64_t renderRequestGeneration { 0 };
    int renderPageIndex { -1 };
    float renderScale { 1.0f };

    bool searchJobScheduled { false };
    bool searchRequestPending { false };
    std::shared_ptr<PdfDocument> searchDocument;
    std::uint64_t searchRequestGeneration { 0 };
    int searchPageIndex { -1 };
    juce::String searchQuery;

    void cancelWork() noexcept
    {
        renderGeneration.fetch_add (1, std::memory_order_release);
        searchGeneration.fetch_add (1, std::memory_order_release);
        cancelPendingUpdate();

        const juce::ScopedLock requestGuard (requestLock);
        renderRequestPending = false;
        searchRequestPending = false;
        renderDocument.reset();
        searchDocument.reset();
        searchQuery.clear();

        const juce::ScopedLock completionGuard (completionLock);
        renderCompletion = {};
        searchCompletion = {};
    }

    void detachOwner() noexcept
    {
        owner = nullptr;
        cancelWork();
    }

    void postRender (std::uint64_t generation, PdfPage page, juce::Image image)
    {
        if (renderGeneration.load (std::memory_order_acquire) != generation)
            return;

        {
            const juce::ScopedLock lock (completionLock);

            if (renderGeneration.load (std::memory_order_acquire) != generation)
                return;

            renderCompletion.pending = true;
            renderCompletion.generation = generation;
            renderCompletion.page = page;
            renderCompletion.image = std::move (image);
        }

        triggerAsyncUpdate();
    }

    void postSearch (std::uint64_t generation,
                     int pageIndex,
                     juce::Array<PdfSearchResult> results)
    {
        if (searchGeneration.load (std::memory_order_acquire) != generation)
            return;

        {
            const juce::ScopedLock lock (completionLock);

            if (searchGeneration.load (std::memory_order_acquire) != generation)
                return;

            searchCompletion.pending = true;
            searchCompletion.generation = generation;
            searchCompletion.pageIndex = pageIndex;
            searchCompletion.results = std::move (results);
        }

        triggerAsyncUpdate();
    }

private:
    struct RenderCompletion
    {
        bool pending { false };
        std::uint64_t generation { 0 };
        PdfPage page;
        juce::Image image;
    };

    struct SearchCompletion
    {
        bool pending { false };
        std::uint64_t generation { 0 };
        int pageIndex { -1 };
        juce::Array<PdfSearchResult> results;
    };

    juce::CriticalSection completionLock;
    RenderCompletion renderCompletion;
    SearchCompletion searchCompletion;

    void handleAsyncUpdate() override
    {
        // Listener/callback publication can destroy the owning widget and release its
        // shared_ptr<RenderState>. Keep this state alive until the callback fully returns.
        const auto keepAlive = shared_from_this();

        RenderCompletion render;
        SearchCompletion search;

        {
            const juce::ScopedLock lock (completionLock);

            if (renderCompletion.pending)
            {
                render = std::move (renderCompletion);
                renderCompletion = {};
            }

            if (searchCompletion.pending)
            {
                search = std::move (searchCompletion);
                searchCompletion = {};
            }
        }

        if (owner != nullptr
            && render.pending
            && renderGeneration.load (std::memory_order_acquire) == render.generation)
        {
            owner->publishPageRender (render.generation,
                                      render.page,
                                      std::move (render.image));
        }

        if (owner != nullptr
            && search.pending
            && searchGeneration.load (std::memory_order_acquire) == search.generation)
        {
            owner->publishSearchResults (search.generation,
                                         search.pageIndex,
                                         std::move (search.results));
        }

        bool hasMore = false;

        {
            const juce::ScopedLock lock (completionLock);
            hasMore = renderCompletion.pending || searchCompletion.pending;
        }

        if (hasMore)
            triggerAsyncUpdate();
    }
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

void PdfDefaultLookAndFeel::drawPdfViewSearchHighlight (juce::Graphics& g,
                                                        juce::Rectangle<float> highlightBounds,
                                                        PdfViewComponent& comp)
{
    g.setColour (resolveColour (comp,
                                PdfViewComponent::searchHighlightColourId,
                                juce::Colour (0x66ffd54f)));
    g.fillRect (highlightBounds);
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

void PdfViewComponent::LookAndFeelMethods::drawPdfViewSearchHighlight (juce::Graphics& g,
                                                                       juce::Rectangle<float> highlightBounds,
                                                                       PdfViewComponent& comp)
{
    PdfDefaultLookAndFeel::getDefaultInstance().drawPdfViewSearchHighlight (g, highlightBounds, comp);
}

//==============================================================================

PdfViewComponent::PdfViewComponent()
    : renderState (std::make_shared<RenderState>())
{
    renderState->owner = this;
}

PdfViewComponent::~PdfViewComponent()
{
    if (renderState != nullptr)
        renderState->detachOwner();

    currentDocument.reset();
    renderState.reset();
}

void PdfViewComponent::loadDocument (juce::String filePath)
{
    try
    {
        auto candidate = std::make_shared<PdfDocument>();

        if (!candidate->open (juce::File (filePath))
            || candidate->getPageCount() <= 0)
        {
            return;
        }

        activateDocument (std::move (candidate));
    }
    catch (const std::bad_alloc&)
    {
        // Transactional failure: keep the current document/view state.
    }
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
    return thereIsADocumentLoaded() ? currentPageCount : -1;
}

int PdfViewComponent::getCurrentPageOnScreen() const
{
    return thereIsADocumentLoaded() ? currentPage : -1;
}

void PdfViewComponent::setPageNumber (int pageNumber)
{
    if (!thereIsADocumentLoaded())
        return;

    if (pageNumber < 1 || pageNumber > currentPageCount || pageNumber == currentPage)
        return;

    const auto pageInfo = currentDocument->getPage (pageNumber - 1);

    if (!pageInfo.isValid())
        return;

    currentPage = pageNumber;
    currentZoom = 1.0f;
    topLeft = {};
    currentPageInfo = pageInfo;
    clampTopLeft();

    invalidatePageCache();
    invalidateSearchResults();
    requestPageRender();
    requestSearchResults();
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
    return thereIsADocumentLoaded() ? currentZoom : -1.0f;
}

void PdfViewComponent::setCurrentPageZoom (float zoom, juce::Point<float> handlePoint)
{
    if (!thereIsADocumentLoaded()
        || !std::isfinite (zoom)
        || !isFinitePoint (handlePoint))
    {
        return;
    }

    const float newZoom = juce::jlimit (minZoom, maxZoom, zoom);

    if (std::abs (newZoom - currentZoom) <= std::numeric_limits<float>::epsilon())
        return;

    const float oldZoom = currentZoom;
    const auto anchorInPage = (handlePoint - topLeft) / oldZoom;

    currentZoom = newZoom;
    topLeft = handlePoint - anchorInPage * newZoom;

    clampTopLeft();
    invalidatePageCache();
    requestPageRender();
}

juce::Point<float> PdfViewComponent::getCurrentPageTopLeftPosition() const
{
    return thereIsADocumentLoaded() ? topLeft : juce::Point<float> {};
}

void PdfViewComponent::setCurrentPageTopLeftPosition (juce::Point<float> newPos)
{
    if (!thereIsADocumentLoaded() || !isFinitePoint (newPos))
        return;

    topLeft = newPos;
    clampTopLeft();
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

void PdfViewComponent::exportCurrentDocument (juce::String withName,
                                              juce::String folderPath) const
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

    try
    {
        auto candidate = std::make_shared<PdfDocument>();

        if (!candidate->open (data, static_cast<size_t> (sizeInBytes))
            || candidate->getPageCount() <= 0)
        {
            return;
        }

        activateDocument (std::move (candidate));
    }
    catch (const std::bad_alloc&)
    {
        // Transactional failure: keep the current document/view state.
    }
}

void PdfViewComponent::getMemoryBlockFromDocument (juce::MemoryBlock& destData)
{
    if (thereIsADocumentLoaded())
        (void) currentDocument->saveToMemoryBlock (destData);
}

void PdfViewComponent::setDocument (std::shared_ptr<PdfDocument> doc)
{
    if (doc == currentDocument)
        return;

    if (doc == nullptr)
    {
        const bool hadDocument = thereIsADocumentLoaded();

        if (renderState != nullptr)
            renderState->cancelWork();

        currentDocument.reset();
        cachedPageImage = {};
        currentPageInfo = {};
        currentPageCount = 0;
        currentPage = 0;
        currentZoom = 1.0f;
        topLeft = {};
        searchQuery.clear();
        searchResults.clearQuick();
        repaint();

        if (hadDocument)
            notifyDocumentClosed();

        return;
    }

    if (!doc->isOpen() || doc->getPageCount() <= 0)
        return;

    activateDocument (std::move (doc));
}

void PdfViewComponent::setSearchQuery (const juce::String& query)
{
    const auto queryBytes = static_cast<std::uint64_t> (query.getNumBytesAsUTF8());

    if (queryBytes > detail::maxSearchQueryUtf8Bytes)
    {
        const bool hadSearchState = searchQuery.isNotEmpty()
                                 || !searchResults.isEmpty();

        searchQuery.clear();
        invalidateSearchResults();

        if (hadSearchState)
            notifySearchResultsChanged();

        return;
    }

    if (query == searchQuery)
        return;

    searchQuery = query;
    invalidateSearchResults();

    if (searchQuery.isEmpty())
    {
        notifySearchResultsChanged();
        return;
    }

    requestSearchResults();
}

void PdfViewComponent::clearSearch()
{
    setSearchQuery ({});
}

int PdfViewComponent::getSearchResultCount() const noexcept
{
    return searchResults.size();
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

    for (const auto& result : searchResults)
    {
        if (result.pageIndex != currentPage - 1)
            continue;

        const auto highlightBounds = pdfBoundsToWidget (currentPageInfo, result.bounds);

        if (!highlightBounds.isEmpty())
            laf.drawPdfViewSearchHighlight (g, highlightBounds, *this);
    }
}

void PdfViewComponent::resized()
{
    updateRasterDeviceScale();
    clampTopLeft();
    repaint();
}

void PdfViewComponent::moved()
{
    updateRasterDeviceScale();
}

void PdfViewComponent::parentHierarchyChanged()
{
    updateRasterDeviceScale();
}

void PdfViewComponent::mouseDown (const juce::MouseEvent& event)
{
    lastDragPosition = event.position;
}

void PdfViewComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (!event.mods.isLeftButtonDown())
        return;

    const auto delta = event.position - lastDragPosition;
    lastDragPosition = event.position;

    if (!isFinitePoint (delta))
        return;

    topLeft += delta;
    clampTopLeft();
    repaint();
}

void PdfViewComponent::mouseWheelMove (const juce::MouseEvent& event,
                                       const juce::MouseWheelDetails& wheel)
{
    if (event.mods.isCommandDown() || event.mods.isCtrlDown())
    {
        const float zoomMultiplier = std::pow (2.0f, wheel.deltaY);
        setCurrentPageZoom (currentZoom * zoomMultiplier, event.position);
        return;
    }

    constexpr float wheelPanScale = 80.0f;
    const juce::Point<float> delta { wheel.deltaX * wheelPanScale,
                                     wheel.deltaY * wheelPanScale };

    if (!isFinitePoint (delta))
        return;

    topLeft += delta;
    clampTopLeft();
    repaint();
}

void PdfViewComponent::mouseMagnify (const juce::MouseEvent& event, float scaleFactor)
{
    if (!std::isfinite (scaleFactor) || scaleFactor <= 0.0f)
        return;

    setCurrentPageZoom (currentZoom * scaleFactor, event.position);
}

PdfViewComponent::LookAndFeelMethods& PdfViewComponent::getLAF()
{
    if (auto* l = dynamic_cast<LookAndFeelMethods*> (&getLookAndFeel()))
        return *l;

    return PdfDefaultLookAndFeel::getDefaultInstance();
}

PdfPage PdfViewComponent::getCurrentPageInfo() const
{
    return thereIsADocumentLoaded() ? currentPageInfo : PdfPage {};
}

void PdfViewComponent::activateDocument (std::shared_ptr<PdfDocument> doc)
{
    currentDocument = std::move (doc);
    currentPageCount = currentDocument != nullptr ? currentDocument->getPageCount() : 0;
    currentPage = currentPageCount > 0 ? 1 : 0;
    currentZoom = 1.0f;
    rasterDeviceScale = 1.0f;
    topLeft = {};
    currentPageInfo = currentPage > 0 ? currentDocument->getPage (0) : PdfPage {};

    searchQuery.clear();
    invalidateSearchResults();
    updateRasterDeviceScale();
    clampTopLeft();

    invalidatePageCache();
    requestPageRender();
    notifyDocumentLoaded();
}

void PdfViewComponent::invalidatePageCache()
{
    if (renderState != nullptr)
        renderState->renderGeneration.fetch_add (1, std::memory_order_release);

    cachedPageImage = {};
    repaint();
}

void PdfViewComponent::requestPageRender()
{
    if (!thereIsADocumentLoaded() || renderState == nullptr)
        return;

    const auto state = renderState;
    bool shouldScheduleJob = false;

    {
        const juce::ScopedLock lock (state->requestLock);

        state->renderDocument = currentDocument;
        state->renderRequestGeneration = state->renderGeneration.load (std::memory_order_acquire);
        state->renderPageIndex = currentPage - 1;
        state->renderScale = clampRasterScaleToSafetyBudget (
            currentPageInfo,
            currentZoom * rasterDeviceScale);
        state->renderRequestPending = true;

        if (!state->renderJobScheduled)
        {
            state->renderJobScheduled = true;
            shouldScheduleJob = true;
        }
    }

    if (!shouldScheduleJob)
        return;

    try
    {
        getPdfRenderPool().addJob ([state]
        {
            for (;;)
            {
                std::shared_ptr<PdfDocument> document;
                std::uint64_t generation = 0;
                int pageIndex = -1;
                float rasterScale = 1.0f;

                {
                    const juce::ScopedLock lock (state->requestLock);

                    if (!state->renderRequestPending)
                    {
                        state->renderJobScheduled = false;
                        return;
                    }

                    document = state->renderDocument;
                    generation = state->renderRequestGeneration;
                    pageIndex = state->renderPageIndex;
                    rasterScale = state->renderScale;
                    state->renderRequestPending = false;
                }

                if (document == nullptr
                    || state->renderGeneration.load (std::memory_order_acquire) != generation)
                {
                    continue;
                }

                const auto page = document->getPage (pageIndex);

                if (!page.isValid()
                    || state->renderGeneration.load (std::memory_order_acquire) != generation)
                {
                    continue;
                }

                auto image = document->renderPage (pageIndex, rasterScale);

                if (state->renderGeneration.load (std::memory_order_acquire) != generation)
                    continue;

                state->postRender (generation, page, std::move (image));
            }
        });
    }
    catch (const std::bad_alloc&)
    {
        const juce::ScopedLock lock (state->requestLock);
        state->renderJobScheduled = false;
    }
}

void PdfViewComponent::publishPageRender (std::uint64_t generation,
                                          PdfPage page,
                                          juce::Image image)
{
    if (renderState == nullptr
        || generation != renderState->renderGeneration.load (std::memory_order_acquire)
        || page.index != currentPage - 1)
    {
        return;
    }

    currentPageInfo = page;
    cachedPageImage = std::move (image);
    clampTopLeft();
    repaint();
}

void PdfViewComponent::invalidateSearchResults()
{
    if (renderState != nullptr)
        renderState->searchGeneration.fetch_add (1, std::memory_order_release);

    searchResults.clearQuick();
    repaint();
}

void PdfViewComponent::requestSearchResults()
{
    if (!thereIsADocumentLoaded()
        || renderState == nullptr
        || searchQuery.isEmpty())
    {
        return;
    }

    const auto state = renderState;
    bool shouldScheduleJob = false;

    {
        const juce::ScopedLock lock (state->requestLock);

        state->searchDocument = currentDocument;
        state->searchRequestGeneration = state->searchGeneration.load (std::memory_order_acquire);
        state->searchPageIndex = currentPage - 1;
        state->searchQuery = searchQuery;
        state->searchRequestPending = true;

        if (!state->searchJobScheduled)
        {
            state->searchJobScheduled = true;
            shouldScheduleJob = true;
        }
    }

    if (!shouldScheduleJob)
        return;

    try
    {
        getPdfRenderPool().addJob ([state]
        {
            for (;;)
            {
                std::shared_ptr<PdfDocument> document;
                std::uint64_t generation = 0;
                int pageIndex = -1;
                juce::String query;

                {
                    const juce::ScopedLock lock (state->requestLock);

                    if (!state->searchRequestPending)
                    {
                        state->searchJobScheduled = false;
                        return;
                    }

                    document = state->searchDocument;
                    generation = state->searchRequestGeneration;
                    pageIndex = state->searchPageIndex;
                    query = state->searchQuery;
                    state->searchRequestPending = false;
                }

                if (document == nullptr
                    || query.isEmpty()
                    || state->searchGeneration.load (std::memory_order_acquire) != generation)
                {
                    continue;
                }

                auto results = document->findText (query, pageIndex);

                if (state->searchGeneration.load (std::memory_order_acquire) != generation)
                    continue;

                state->postSearch (generation, pageIndex, std::move (results));
            }
        });
    }
    catch (const std::bad_alloc&)
    {
        const juce::ScopedLock lock (state->requestLock);
        state->searchJobScheduled = false;
    }
}

void PdfViewComponent::publishSearchResults (std::uint64_t generation,
                                             int pageIndex,
                                             juce::Array<PdfSearchResult> results)
{
    if (renderState == nullptr
        || generation != renderState->searchGeneration.load (std::memory_order_acquire)
        || pageIndex != currentPage - 1)
    {
        return;
    }

    searchResults = std::move (results);
    repaint();
    notifySearchResultsChanged();
}

void PdfViewComponent::updateRasterDeviceScale()
{
    float newScale = juce::Component::getApproximateScaleFactorForComponent (this);

    if (const auto* display = juce::Desktop::getInstance()
                                  .getDisplays()
                                  .getDisplayForRect (getScreenBounds()))
    {
        newScale *= static_cast<float> (display->scale);
    }

    if (!std::isfinite (newScale) || newScale <= 0.0f)
        newScale = 1.0f;

    newScale = juce::jlimit (0.25f, 8.0f, newScale);

    if (std::abs (newScale - rasterDeviceScale) <= 0.001f)
        return;

    rasterDeviceScale = newScale;

    if (thereIsADocumentLoaded())
    {
        invalidatePageCache();
        requestPageRender();
    }
}

void PdfViewComponent::clampTopLeft()
{
    const auto page = getCurrentPageInfo();

    if (!page.isValid()
        || !std::isfinite (currentZoom)
        || currentZoom <= 0.0f)
    {
        return;
    }

    const auto displaySize = page.getDisplaySize() * currentZoom;
    const float viewWidth = static_cast<float> (getWidth());
    const float viewHeight = static_cast<float> (getHeight());

    if (displaySize.x <= viewWidth)
        topLeft.x = (viewWidth - displaySize.x) * 0.5f;
    else
        topLeft.x = juce::jlimit (viewWidth * minVisibleFraction - displaySize.x,
                                  viewWidth * (1.0f - minVisibleFraction),
                                  topLeft.x);

    if (displaySize.y <= viewHeight)
        topLeft.y = (viewHeight - displaySize.y) * 0.5f;
    else
        topLeft.y = juce::jlimit (viewHeight * minVisibleFraction - displaySize.y,
                                  viewHeight * (1.0f - minVisibleFraction),
                                  topLeft.y);
}

juce::Rectangle<float> PdfViewComponent::pdfBoundsToWidget (const PdfPage& page,
                                                            juce::Rectangle<float> pdfBounds) const
{
    const auto displayBounds = page.getDisplayBounds (pdfBounds);

    if (displayBounds.isEmpty())
        return {};

    return displayBounds * currentZoom + topLeft;
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

void PdfViewComponent::notifyDocumentClosed()
{
    juce::Component::BailOutChecker checker (this);
    listeners.callChecked (checker, [this] (Listener& listener)
    {
        listener.pdfDocumentClosed (this);
    });

    if (checker.shouldBailOut())
        return;

    if (onDocumentClosed)
        onDocumentClosed();
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

void PdfViewComponent::notifySearchResultsChanged()
{
    const int resultCount = searchResults.size();
    juce::Component::BailOutChecker checker (this);

    listeners.callChecked (checker, [this, resultCount] (Listener& listener)
    {
        listener.pdfSearchResultsChanged (this, resultCount);
    });

    if (checker.shouldBailOut())
        return;

    if (onSearchResultsChanged)
        onSearchResultsChanged (resultCount);
}

} // namespace ayra
