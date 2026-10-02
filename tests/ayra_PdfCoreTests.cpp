namespace ayra::tests
{

#ifndef AYRA_PDF_HEADLESS
using LegacyLoadDocumentSignature = void (PDFComponent::*) (juce::String);
using LegacyDocumentLoadedSignature = bool (PDFComponent::*) () const;
using LegacyIntGetterSignature = int (PDFComponent::*) () const;
using LegacyPageSetterSignature = void (PDFComponent::*) (int);
using LegacyFloatGetterSignature = float (PDFComponent::*) () const;
using LegacyZoomSetterSignature = void (PDFComponent::*) (float, juce::Point<float>);
using LegacyPointGetterSignature = juce::Point<float> (PDFComponent::*) () const;
using LegacyPointSetterSignature = void (PDFComponent::*) (juce::Point<float>);
using LegacyBoundsGetterSignature = juce::Rectangle<float> (PDFComponent::*) () const;
using LegacyExportDocumentSignature = void (PDFComponent::*) (juce::String, juce::String) const;
using LegacyLoadMemorySignature = void (PDFComponent::*) (const void*, int);
using LegacySaveMemorySignature = void (PDFComponent::*) (juce::MemoryBlock&);

static_assert (std::is_same_v<PDFComponent, PdfViewComponent>);
static_assert (std::is_base_of_v<juce::Component, PDFComponent>);

static_assert (std::is_same_v<
    decltype (static_cast<LegacyLoadDocumentSignature> (&PDFComponent::loadDocument)),
    LegacyLoadDocumentSignature>);
static_assert (std::is_same_v<
    decltype (static_cast<LegacyDocumentLoadedSignature> (&PDFComponent::thereIsADocumentLoaded)),
    LegacyDocumentLoadedSignature>);
static_assert (std::is_same_v<
    decltype (static_cast<LegacyIntGetterSignature> (&PDFComponent::getTotPagesNum)),
    LegacyIntGetterSignature>);
static_assert (std::is_same_v<
    decltype (static_cast<LegacyIntGetterSignature> (&PDFComponent::getCurrentPageOnScreen)),
    LegacyIntGetterSignature>);
static_assert (std::is_same_v<
    decltype (static_cast<LegacyPageSetterSignature> (&PDFComponent::setPageNumber)),
    LegacyPageSetterSignature>);
static_assert (std::is_same_v<
    decltype (static_cast<LegacyFloatGetterSignature> (&PDFComponent::getDocumentWidth)),
    LegacyFloatGetterSignature>);
static_assert (std::is_same_v<
    decltype (static_cast<LegacyFloatGetterSignature> (&PDFComponent::getDocumentHeight)),
    LegacyFloatGetterSignature>);
static_assert (std::is_same_v<
    decltype (static_cast<LegacyFloatGetterSignature> (&PDFComponent::getCurrentPageZoom)),
    LegacyFloatGetterSignature>);
static_assert (std::is_same_v<
    decltype (static_cast<LegacyZoomSetterSignature> (&PDFComponent::setCurrentPageZoom)),
    LegacyZoomSetterSignature>);
static_assert (std::is_same_v<
    decltype (static_cast<LegacyPointGetterSignature> (&PDFComponent::getCurrentPageTopLeftPosition)),
    LegacyPointGetterSignature>);
static_assert (std::is_same_v<
    decltype (static_cast<LegacyPointSetterSignature> (&PDFComponent::setCurrentPageTopLeftPosition)),
    LegacyPointSetterSignature>);
static_assert (std::is_same_v<
    decltype (static_cast<LegacyBoundsGetterSignature> (&PDFComponent::getCurrentPageBounds)),
    LegacyBoundsGetterSignature>);
static_assert (std::is_same_v<
    decltype (static_cast<LegacyExportDocumentSignature> (&PDFComponent::exportCurrentDocument)),
    LegacyExportDocumentSignature>);
static_assert (std::is_same_v<
    decltype (static_cast<LegacyLoadMemorySignature> (&PDFComponent::loadDocumentFromMemoryBlock)),
    LegacyLoadMemorySignature>);
static_assert (std::is_same_v<
    decltype (static_cast<LegacySaveMemorySignature> (&PDFComponent::getMemoryBlockFromDocument)),
    LegacySaveMemorySignature>);
#endif

namespace
{

constexpr int fixtureRotations[] { 0, 90, 180, 270 };

void appendPdfAscii (juce::MemoryOutputStream& output, const juce::String& text)
{
    (void) output.writeText (text, false, false, nullptr);
}

juce::MemoryBlock makePdfFixture()
{
    juce::MemoryOutputStream output;
    std::vector<juce::int64> offsets (9, 0);

    const auto beginObject = [&] (int objectNumber)
    {
        offsets[static_cast<size_t> (objectNumber)] = output.getPosition();
        appendPdfAscii (output, juce::String (objectNumber) + " 0 obj\n");
    };

    appendPdfAscii (output, "%PDF-1.4\n");

    beginObject (1);
    appendPdfAscii (output, "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    beginObject (2);
    appendPdfAscii (output,
                    "<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R 6 0 R] /Count 4 >>\n"
                    "endobj\n");

    for (int page = 0; page < 4; ++page)
    {
        const int objectNumber = 3 + page;
        beginObject (objectNumber);

        appendPdfAscii (
            output,
            juce::String ("<< /Type /Page /Parent 2 0 R "
                          "/MediaBox [10 20 410 620] "
                          "/CropBox [30 50 330 550] "
                          "/Rotate ")
                + juce::String (fixtureRotations[page])
                + " /Resources << /Font << /F1 8 0 R >> >> "
                  "/Contents 7 0 R >>\n"
                  "endobj\n");
    }

    const juce::String content (
        "q\n"
        "1 0 0 rg\n"
        "40 60 20 20 re f\n"
        "0 1 0 rg\n"
        "300 60 20 20 re f\n"
        "0 0 1 rg\n"
        "40 520 20 20 re f\n"
        "1 0 1 rg\n"
        "300 520 20 20 re f\n"
        "Q\n"
        "BT\n"
        "/F1 24 Tf\n"
        "60 100 Td\n"
        "(HELLO AYRA PDF) Tj\n"
        "ET\n");

    beginObject (7);
    appendPdfAscii (
        output,
        juce::String ("<< /Length ")
            + juce::String (static_cast<juce::int64> (content.getNumBytesAsUTF8()))
            + " >>\nstream\n");
    appendPdfAscii (output, content);
    appendPdfAscii (output, "endstream\nendobj\n");

    beginObject (8);
    appendPdfAscii (output,
                    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\n"
                    "endobj\n");

    const auto xrefOffset = output.getPosition();

    appendPdfAscii (output, "xref\n0 9\n");
    appendPdfAscii (output, "0000000000 65535 f \n");

    for (int objectNumber = 1; objectNumber <= 8; ++objectNumber)
    {
        appendPdfAscii (
            output,
            juce::String (offsets[static_cast<size_t> (objectNumber)])
                .paddedLeft ('0', 10)
                + " 00000 n \n");
    }

    appendPdfAscii (
        output,
        juce::String ("trailer\n"
                      "<< /Size 9 /Root 1 0 R >>\n"
                      "startxref\n")
            + juce::String (xrefOffset)
            + "\n%%EOF\n");

    return output.getMemoryBlock();
}

juce::MemoryBlock makeWinAnsiTextFixture()
{
    juce::MemoryOutputStream output;
    std::vector<juce::int64> offsets (7, 0);

    const auto beginObject = [&] (int objectNumber)
    {
        offsets[static_cast<size_t> (objectNumber)] = output.getPosition();
        appendPdfAscii (output, juce::String (objectNumber) + " 0 obj\n");
    };

    appendPdfAscii (output, "%PDF-1.4\n");

    beginObject (1);
    appendPdfAscii (output, "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    beginObject (2);
    appendPdfAscii (
        output,
        "<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");

    beginObject (3);
    appendPdfAscii (
        output,
        "<< /Type /Page /Parent 2 0 R "
        "/MediaBox [0 0 300 400] "
        "/Resources << /Font << /F1 5 0 R >> >> "
        "/Contents 4 0 R >>\nendobj\n");

    // PDF literal string uses one raw WinAnsi byte 0xC9 for capital E-acute.
    // Keep this stream byte-oriented: appendPdfAscii would UTF-8 encode it.
    static constexpr char content[] =
        "BT\n"
        "/F1 20 Tf\n"
        "40 200 Td\n"
        "(CAF\311 AYRA) Tj\n"
        "ET\n";

    beginObject (4);
    appendPdfAscii (
        output,
        juce::String ("<< /Length ")
            + juce::String (static_cast<juce::int64> (sizeof (content) - 1))
            + " >>\nstream\n");
    output.write (content, sizeof (content) - 1);
    appendPdfAscii (output, "endstream\nendobj\n");

    beginObject (5);
    appendPdfAscii (
        output,
        "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica "
        "/Encoding /WinAnsiEncoding >>\nendobj\n");

    beginObject (6);
    appendPdfAscii (output, "<< /Producer (Ayra Test) >>\nendobj\n");

    const auto xrefOffset = output.getPosition();

    appendPdfAscii (output, "xref\n0 7\n");
    appendPdfAscii (output, "0000000000 65535 f \n");

    for (int objectNumber = 1; objectNumber <= 6; ++objectNumber)
    {
        appendPdfAscii (
            output,
            juce::String (offsets[static_cast<size_t> (objectNumber)])
                .paddedLeft ('0', 10)
                + " 00000 n \n");
    }

    appendPdfAscii (
        output,
        juce::String ("trailer\n"
                      "<< /Size 7 /Root 1 0 R >>\n"
                      "startxref\n")
            + juce::String (xrefOffset)
            + "\n%%EOF\n");

    return output.getMemoryBlock();
}

bool rectangleApproximatelyEquals (juce::Rectangle<float> actual,
                                   juce::Rectangle<float> expected,
                                   float epsilon = 0.001f)
{
    return std::abs (actual.getX() - expected.getX()) <= epsilon
        && std::abs (actual.getY() - expected.getY()) <= epsilon
        && std::abs (actual.getWidth() - expected.getWidth()) <= epsilon
        && std::abs (actual.getHeight() - expected.getHeight()) <= epsilon;
}

bool colourApproximatelyEquals (juce::Colour actual,
                                juce::Colour expected,
                                int tolerance = 8)
{
    return std::abs (static_cast<int> (actual.getRed())
                     - static_cast<int> (expected.getRed())) <= tolerance
        && std::abs (static_cast<int> (actual.getGreen())
                     - static_cast<int> (expected.getGreen())) <= tolerance
        && std::abs (static_cast<int> (actual.getBlue())
                     - static_cast<int> (expected.getBlue())) <= tolerance;
}

juce::Colour sampleDisplayRectCentre (const juce::Image& image,
                                      juce::Rectangle<float> displayBounds)
{
    if (! image.isValid() || displayBounds.isEmpty())
        return {};

    const int x = juce::jlimit (0,
                                image.getWidth() - 1,
                                juce::roundToInt (displayBounds.getCentreX()));
    const int y = juce::jlimit (0,
                                image.getHeight() - 1,
                                juce::roundToInt (displayBounds.getCentreY()));
    return image.getPixelAt (x, y);
}

#ifndef AYRA_PDF_HEADLESS
bool waitForPdfWorkerIdle (int timeoutMilliseconds)
{
    const auto deadline = juce::Time::getMillisecondCounter()
                        + static_cast<juce::uint32> (timeoutMilliseconds);

    for (;;)
    {
        if (getPdfRenderPool().getNumJobs() == 0)
            return true;

        auto* job = getPdfRenderPool().getJob (0);

        if (job == nullptr)
            continue;

        const auto now = juce::Time::getMillisecondCounter();

        if (now >= deadline)
            return false;

        const int remaining = static_cast<int> (deadline - now);

        if (!getPdfRenderPool().waitForJobToFinish (job, remaining))
            return false;
    }
}

bool drainPdfAsyncPublications (PdfViewComponent& view,
                                int minimumSearchResults,
                                int maxAttempts = 16)
{
    if (!waitForPdfWorkerIdle (5000))
        return false;

    for (int attempt = 0; attempt < maxAttempts; ++attempt)
    {
        if (view.getSearchResultCount() >= minimumSearchResults)
            return true;

        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
    }

    return view.getSearchResultCount() >= minimumSearchResults;
}
#endif

} // namespace

class PdfCoreTests final : public juce::UnitTest
{
public:
    PdfCoreTests()
        : juce::UnitTest ("PDF core contract", "ayra_pdf")
    {
    }

    void runTest() override
    {
       #ifndef AYRA_PDF_HEADLESS
        beginTest ("PDFComponent compatibility name preserves empty-state sentinels");

        {
            PDFComponent compatibilityView;
            expect (! compatibilityView.thereIsADocumentLoaded());
            expectEquals (compatibilityView.getTotPagesNum(), -1);
            expectEquals (compatibilityView.getCurrentPageOnScreen(), -1);
            expectWithinAbsoluteError (compatibilityView.getCurrentPageZoom(), -1.0f, 0.0f);
            expectEquals (compatibilityView.getDocumentWidth(), 0.0f);
            expectEquals (compatibilityView.getDocumentHeight(), 0.0f);
            expect (compatibilityView.getCurrentPageBounds().isEmpty());
            expect (compatibilityView.getCurrentPageTopLeftPosition() == juce::Point<float> {});
        }
       #endif

        beginTest ("PdfPage display geometry is canonical for all rotations");

        PdfPage page;
        page.index = 0;
        page.bounds = { 30.0f, 50.0f, 300.0f, 500.0f };

        const juce::Rectangle<float> source { 60.0f, 100.0f, 40.0f, 20.0f };

        struct RotationExpectation
        {
            int rotation;
            juce::Point<float> displaySize;
            juce::Rectangle<float> displayBounds;
        };

        const RotationExpectation expectations[]
        {
            { 0,   { 300.0f, 500.0f }, { 30.0f, 430.0f, 40.0f, 20.0f } },
            { 90,  { 500.0f, 300.0f }, { 50.0f, 30.0f, 20.0f, 40.0f } },
            { 180, { 300.0f, 500.0f }, { 230.0f, 50.0f, 40.0f, 20.0f } },
            { 270, { 500.0f, 300.0f }, { 430.0f, 230.0f, 20.0f, 40.0f } }
        };

        for (const auto& expectation : expectations)
        {
            page.rotation = expectation.rotation;
            expect (page.getDisplaySize() == expectation.displaySize);
            expect (rectangleApproximatelyEquals (page.getDisplayBounds (source),
                                                   expectation.displayBounds));
        }

        beginTest ("Display geometry clips to visible page box");

        page.rotation = 0;
        expect (rectangleApproximatelyEquals (
            page.getDisplayBounds ({ 20.0f, 40.0f, 30.0f, 30.0f }),
            { 0.0f, 480.0f, 20.0f, 20.0f }));
        expect (page.getDisplayBounds ({ -100.0f, -100.0f, 10.0f, 10.0f }).isEmpty());

        beginTest ("Raster scale is capped without changing visual zoom semantics");

        {
            PdfPage a4;
            a4.index = 0;
            a4.bounds = { 0.0f, 0.0f, 595.0f, 842.0f };
            a4.rotation = 0;

            const float desired = 20.0f;
            const float safe = clampRasterScaleToSafetyBudget (a4, desired);
            const auto displaySize = a4.getDisplaySize();

            const auto rasterWidth = static_cast<std::uint64_t> (
                std::ceil (static_cast<double> (displaySize.x) * safe));
            const auto rasterHeight = static_cast<std::uint64_t> (
                std::ceil (static_cast<double> (displaySize.y) * safe));

            expect (safe > 0.0f && safe < desired);
            expect (rasterWidth <= static_cast<std::uint64_t> (detail::maxRasterDimension));
            expect (rasterHeight <= static_cast<std::uint64_t> (detail::maxRasterDimension));
            expect (rasterWidth * rasterHeight <= detail::maxRasterPixels);

            const double exactPixelLimit = std::sqrt (
                static_cast<double> (detail::maxRasterPixels)
                / (595.0 * 842.0));
            const float edgeSafe = clampRasterScaleToSafetyBudget (
                a4,
                static_cast<float> (exactPixelLimit));
            const auto edgeWidth = static_cast<std::uint64_t> (
                std::ceil (595.0 * static_cast<double> (edgeSafe)));
            const auto edgeHeight = static_cast<std::uint64_t> (
                std::ceil (842.0 * static_cast<double> (edgeSafe)));

            expect (edgeWidth <= static_cast<std::uint64_t> (detail::maxRasterDimension));
            expect (edgeHeight <= static_cast<std::uint64_t> (detail::maxRasterDimension));
            expect (edgeWidth * edgeHeight <= detail::maxRasterPixels);
        }

        beginTest ("Generated fixture opens with visible box and rotations");

        const auto fixture = makePdfFixture();
        expect (fixture.getSize() > 0);

        PdfDocument document;
        expect (document.open (fixture.getData(), fixture.getSize()));
        expect (document.isOpen());
        expectEquals (document.getPageCount(), 4);

       #ifndef AYRA_PDF_HEADLESS
        beginTest ("Widget load/close notifications are balanced");

        {
            struct Listener final : PdfViewComponent::Listener
            {
                int loaded { 0 };
                int closed { 0 };
                int searchChanged { 0 };

                void pdfPageChanged (PdfViewComponent*, int) override {}
                void pdfDocumentLoaded (PdfViewComponent*) override { ++loaded; }
                void pdfDocumentClosed (PdfViewComponent*) override { ++closed; }
                void pdfSearchResultsChanged (PdfViewComponent*, int) override { ++searchChanged; }
            };

            auto shared = std::make_shared<PdfDocument>();
            expect (shared->open (fixture.getData(), fixture.getSize()));

            PdfViewComponent view;
            Listener listener;
            int callbackLoaded = 0;
            int callbackClosed = 0;

            view.addListener (&listener);
            view.onDocumentLoaded = [&] { ++callbackLoaded; };
            view.onDocumentClosed = [&] { ++callbackClosed; };

            view.setDocument (shared);
            expect (view.thereIsADocumentLoaded());
            expectEquals (listener.loaded, 1);
            expectEquals (callbackLoaded, 1);
            expectEquals (listener.closed, 0);
            expectEquals (callbackClosed, 0);

            view.setCurrentPageZoom (2.0f, { 0.0f, 0.0f });
            expectWithinAbsoluteError (view.getCurrentPageZoom(), 2.0f, 0.0001f);
            view.setPageNumber (2);
            expectEquals (view.getCurrentPageOnScreen(), 2);
            expectWithinAbsoluteError (view.getCurrentPageZoom(), 1.0f, 0.0001f);

            view.setSearchQuery ("AYRA");
            const int searchNotificationsBeforeOversize = listener.searchChanged;
            const auto oversizedWidgetQuery = juce::String::repeatedString (
                "x",
                static_cast<int> (detail::maxSearchQueryUtf8Bytes + 1u));
            view.setSearchQuery (oversizedWidgetQuery);
            expectEquals (view.getSearchResultCount(), 0);
            expectEquals (listener.searchChanged,
                          searchNotificationsBeforeOversize + 1);

            static constexpr char invalidWidgetPdf[] = "not a pdf";
            view.loadDocumentFromMemoryBlock (
                invalidWidgetPdf,
                static_cast<int> (sizeof (invalidWidgetPdf) - 1));
            expect (view.thereIsADocumentLoaded());
            expectEquals (view.getCurrentPageOnScreen(), 2);
            expectEquals (listener.loaded, 1);
            expectEquals (callbackLoaded, 1);

            view.loadDocument (
                juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getNonexistentChildFile ("ayra_pdf_missing", ".pdf", false)
                    .getFullPathName());
            expect (view.thereIsADocumentLoaded());
            expectEquals (view.getCurrentPageOnScreen(), 2);
            expectEquals (listener.loaded, 1);
            expectEquals (callbackLoaded, 1);

            view.setSearchQuery ("AYRA");
            expect (drainPdfAsyncPublications (view, 1));
            expect (view.getSearchResultCount() > 0);

            auto replacementDocument = std::make_shared<PdfDocument>();
            expect (replacementDocument->open (
                fixture.getData(),
                fixture.getSize()));

            const int searchNotificationsBeforeReplacement = listener.searchChanged;
            view.setDocument (replacementDocument);

            expect (view.thereIsADocumentLoaded());
            expectEquals (listener.loaded, 2);
            expectEquals (callbackLoaded, 2);
            expectEquals (view.getSearchResultCount(), 0);
            expectEquals (listener.searchChanged,
                          searchNotificationsBeforeReplacement + 1);

            view.setSearchQuery ("AYRA");
            expect (drainPdfAsyncPublications (view, 1));
            expect (view.getSearchResultCount() > 0);

            const int searchNotificationsBeforeClose = listener.searchChanged;

            view.setDocument (nullptr);
            expect (! view.thereIsADocumentLoaded());
            expectEquals (view.getTotPagesNum(), -1);
            expectEquals (view.getCurrentPageOnScreen(), -1);
            expectEquals (listener.closed, 1);
            expectEquals (callbackClosed, 1);
            expectEquals (view.getSearchResultCount(), 0);
            expectEquals (listener.searchChanged,
                          searchNotificationsBeforeClose + 1);

            view.setDocument (nullptr);
            expectEquals (listener.closed, 1);
            expectEquals (callbackClosed, 1);

            view.removeListener (&listener);
        }
       #endif

       #ifndef AYRA_PDF_HEADLESS
        beginTest ("Widget teardown detaches in-flight render and search work");

        {
            for (int iteration = 0; iteration < 8; ++iteration)
            {
                auto shared = std::make_shared<PdfDocument>();
                expect (shared->open (fixture.getData(), fixture.getSize()));

                auto view = std::make_unique<PdfViewComponent>();
                view->setBounds (0, 0, 800, 600);
                view->setDocument (shared);
                view->setSearchQuery ("AYRA");
                view->setCurrentPageZoom (
                    4.0f,
                    juce::Point<float> { 400.0f, 300.0f });

                // Destruction must detach the raw Message-Thread owner while
                // shared RenderState/document snapshots keep worker lifetime safe.
                view.reset();
            }

            expect (getPdfRenderPool().removeAllJobs (false, 5000));
            expectEquals (getPdfRenderPool().getNumJobs(), 0);
        }
       #endif

        for (int pageIndex = 0; pageIndex < 4; ++pageIndex)
        {
            const auto info = document.getPage (pageIndex);
            expect (info.isValid());
            expectEquals (info.index, pageIndex);
            expectEquals (info.rotation, fixtureRotations[pageIndex]);
            expect (rectangleApproximatelyEquals (info.bounds,
                                                   { 30.0f, 50.0f, 300.0f, 500.0f }));

            const auto expectedSize = info.getDisplaySize();
            const auto image = document.renderPage (pageIndex, 1.0f);
            expect (image.isValid());
            expectEquals (image.getWidth(), juce::roundToInt (expectedSize.x));
            expectEquals (image.getHeight(), juce::roundToInt (expectedSize.y));

            struct Marker
            {
                juce::Rectangle<float> pageBounds;
                juce::Colour colour;
            };

            const Marker markers[]
            {
                { { 40.0f, 60.0f, 20.0f, 20.0f }, juce::Colours::red },
                { { 300.0f, 60.0f, 20.0f, 20.0f }, juce::Colours::lime },
                { { 40.0f, 520.0f, 20.0f, 20.0f }, juce::Colours::blue },
                { { 300.0f, 520.0f, 20.0f, 20.0f }, juce::Colours::magenta }
            };

            for (const auto& marker : markers)
            {
                const auto displayBounds = info.getDisplayBounds (marker.pageBounds);
                expect (! displayBounds.isEmpty());
                expect (colourApproximatelyEquals (
                    sampleDisplayRectCentre (image, displayBounds),
                    marker.colour));
            }

            const auto text = document.extractText (pageIndex);
            expect (text.contains ("HELLO AYRA PDF"));

            const auto matches = document.findText ("aYrA", pageIndex);
            expectEquals (matches.size(), 1);

            if (!matches.isEmpty())
            {
                expectEquals (matches[0].pageIndex, pageIndex);
                expect (matches[0].text.equalsIgnoreCase ("AYRA"));
                expect (! matches[0].bounds.isEmpty());
                expect (info.bounds.intersects (matches[0].bounds));
            }
        }

        beginTest ("Whole-document search is case-insensitive and page-complete");

        const auto allMatches = document.findText ("ayra");
        expectEquals (allMatches.size(), 4);

        beginTest ("WinAnsi accented text extracts and searches consistently");

        {
            const auto unicodeFixture = makeWinAnsiTextFixture();
            expect (unicodeFixture.getSize() > 0);

            PdfDocument unicodeDocument;
            expect (unicodeDocument.open (
                unicodeFixture.getData(),
                unicodeFixture.getSize()));

            const auto expectedWord = juce::String::fromUTF8 ("CAF\xC3\x89");
            const auto query = juce::String::fromUTF8 ("caf\xC3\xA9");
            const auto extracted = unicodeDocument.extractText (0);
            const auto matches = unicodeDocument.findText (query, 0);

            expect (extracted.contains (expectedWord));
            expectEquals (matches.size(), 1);

            if (!matches.isEmpty())
                expect (matches[0].text.equalsIgnoreCase (query));
        }

        beginTest ("Save is byte-preserving");

        juce::MemoryBlock saved;
        expect (document.saveToMemoryBlock (saved));
        expect (saved == fixture);

        beginTest ("Owned source snapshot survives external file mutation");

        {
            auto sourceFile = juce::File::getSpecialLocation (
                                  juce::File::tempDirectory)
                                  .getNonexistentChildFile (
                                      "ayra_pdf_source_identity",
                                      ".pdf",
                                      false);

            expect (sourceFile.replaceWithData (
                fixture.getData(),
                fixture.getSize()));

            PdfDocument fileDocument;
            expect (fileDocument.open (sourceFile));
            expect (fileDocument.sourceBytesEqualFile (sourceFile));

            const auto changedBytes = makeWinAnsiTextFixture();
            expect (changedBytes.getSize() > 0);
            expect (sourceFile.replaceWithData (
                changedBytes.getData(),
                changedBytes.getSize()));

            expect (! fileDocument.sourceBytesEqualFile (sourceFile));

            const auto preservedText = fileDocument.extractText (0);
            const auto preservedMatches = fileDocument.findText ("AYRA", 0);
            expect (preservedText.contains ("HELLO AYRA PDF"));
            expectEquals (preservedMatches.size(), 1);

            juce::MemoryBlock preservedSnapshot;
            expect (fileDocument.saveToMemoryBlock (preservedSnapshot));
            expect (preservedSnapshot == fixture);

            expect (sourceFile.replaceWithData (
                fixture.getData(),
                fixture.getSize()));
            expect (fileDocument.sourceBytesEqualFile (sourceFile));

            (void) sourceFile.deleteFile();
        }

        beginTest ("Failed replacement load is transactional");

        static constexpr char invalidPdf[] = "not a pdf";
        expect (! document.open (invalidPdf, sizeof (invalidPdf) - 1));
        expect (document.isOpen());
        expectEquals (document.getPageCount(), 4);

        juce::MemoryBlock afterFailure;
        expect (document.saveToMemoryBlock (afterFailure));
        expect (afterFailure == fixture);

        beginTest ("Truncated PDF replacement is rejected transactionally");

        const size_t truncatedBytes = std::min<size_t> (32u, fixture.getSize());
        expect (truncatedBytes > 0);
        expect (! document.open (fixture.getData(), truncatedBytes));
        expect (document.isOpen());
        expectEquals (document.getPageCount(), 4);

        juce::MemoryBlock afterTruncatedFailure;
        expect (document.saveToMemoryBlock (afterTruncatedFailure));
        expect (afterTruncatedFailure == fixture);

        beginTest ("Resource budgets fail before touching oversized payload memory");

        expectEquals (
            PdfDocument::getMaximumDocumentBytes(),
            detail::maxDocumentBytes);

        static constexpr char oneByte = 'x';
        const auto oversizedDocumentBytes = static_cast<size_t> (
            PdfDocument::getMaximumDocumentBytes() + 1u);
        expect (! document.open (&oneByte, oversizedDocumentBytes));
        expect (document.isOpen());
        expectEquals (document.getPageCount(), 4);

        const auto oversizedQuery = juce::String::repeatedString (
            "x",
            static_cast<int> (detail::maxSearchQueryUtf8Bytes + 1u));
        expect (document.findText (oversizedQuery, 0).isEmpty());

        beginTest ("Concurrent renderer calls are serialized safely");

        std::atomic<bool> concurrentRenderOk { true };
        std::atomic<bool> concurrentSearchOk { true };

        std::thread renderThread ([&]
        {
            for (int iteration = 0; iteration < 8; ++iteration)
            {
                const auto image = document.renderPage (iteration % 4, 0.5f);

                if (! image.isValid())
                    concurrentRenderOk.store (false, std::memory_order_release);
            }
        });

        std::thread searchThread ([&]
        {
            for (int iteration = 0; iteration < 8; ++iteration)
            {
                const auto results = document.findText ("AYRA", iteration % 4);

                if (results.size() != 1)
                    concurrentSearchOk.store (false, std::memory_order_release);
            }
        });

        renderThread.join();
        searchThread.join();

        expect (concurrentRenderOk.load (std::memory_order_acquire));
        expect (concurrentSearchOk.load (std::memory_order_acquire));

       #if JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID
        beginTest ("PDFium process lock serializes concurrent multi-instance work");

        constexpr int workerCount = 4;
        constexpr int iterationsPerWorker = 4;
        std::atomic<int> completedIterations { 0 };
        std::vector<std::thread> workers;
        workers.reserve (workerCount);

        for (int worker = 0; worker < workerCount; ++worker)
        {
            workers.emplace_back ([&]
            {
                for (int iteration = 0; iteration < iterationsPerWorker; ++iteration)
                {
                    PdfDocument localDocument;

                    if (! localDocument.open (fixture.getData(), fixture.getSize()))
                        continue;

                    const auto image = localDocument.renderPage (iteration % 4, 0.25f);
                    const auto matches = localDocument.findText ("AYRA", iteration % 4);

                    if (image.isValid() && matches.size() == 1)
                        completedIterations.fetch_add (1, std::memory_order_relaxed);
                }
            });
        }

        for (auto& worker : workers)
            worker.join();

        expectEquals (completedIterations.load (std::memory_order_relaxed),
                      workerCount * iterationsPerWorker);

        beginTest ("PDFium library reinitializes after zero live instances");

        constexpr int lifecycleBatches = 8;
        bool lifecycleOk = true;

        for (int batch = 0; batch < lifecycleBatches; ++batch)
        {
            {
                PdfDocument first;

                if (!first.open (fixture.getData(), fixture.getSize())
                    || !first.renderPage (batch % 4, 0.25f).isValid())
                {
                    lifecycleOk = false;
                }
            }

            // At this point no PdfDocument from this batch is alive, so the
            // process-wide renderer count must have returned to zero.
            {
                PdfDocument second;

                if (!second.open (fixture.getData(), fixture.getSize())
                    || second.findText ("AYRA", batch % 4).size() != 1)
                {
                    lifecycleOk = false;
                }
            }
        }

        expect (lifecycleOk);
       #endif

        beginTest ("Invalid indexes and raster scales fail closed");

        expect (! document.getPage (-1).isValid());
        expect (! document.getPage (4).isValid());
        expect (! document.renderPage (-1, 1.0f).isValid());
        expect (! document.renderPage (0, 0.0f).isValid());
        expect (! document.renderPage (0, std::numeric_limits<float>::infinity()).isValid());

        document.close();
        expect (! document.isOpen());
        expectEquals (document.getPageCount(), 0);

        beginTest ("Save failure leaves destination memory unchanged");

        static constexpr char sentinelBytes[] = "sentinel";
        juce::MemoryBlock destination (sentinelBytes, sizeof (sentinelBytes) - 1);
        const auto originalDestination = destination;

        expect (! document.saveToMemoryBlock (destination));
        expect (destination == originalDestination);
    }
};

static PdfCoreTests pdfCoreTests;

} // namespace ayra::tests
