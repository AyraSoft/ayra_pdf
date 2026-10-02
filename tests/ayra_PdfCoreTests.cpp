namespace ayra::tests
{

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

        beginTest ("Generated fixture opens with visible box and rotations");

        const auto fixture = makePdfFixture();
        expect (fixture.getSize() > 0);

        PdfDocument document;
        expect (document.open (fixture.getData(), fixture.getSize()));
        expect (document.isOpen());
        expectEquals (document.getPageCount(), 4);

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

        beginTest ("Save is byte-preserving");

        juce::MemoryBlock saved;
        expect (document.saveToMemoryBlock (saved));
        expect (saved == fixture);

        beginTest ("Failed replacement load is transactional");

        static constexpr char invalidPdf[] = "not a pdf";
        expect (! document.open (invalidPdf, sizeof (invalidPdf) - 1));
        expect (document.isOpen());
        expectEquals (document.getPageCount(), 4);

        juce::MemoryBlock afterFailure;
        expect (document.saveToMemoryBlock (afterFailure));
        expect (afterFailure == fixture);

        beginTest ("Invalid indexes and raster scales fail closed");

        expect (! document.getPage (-1).isValid());
        expect (! document.getPage (4).isValid());
        expect (! document.renderPage (-1, 1.0f).isValid());
        expect (! document.renderPage (0, 0.0f).isValid());
        expect (! document.renderPage (0, std::numeric_limits<float>::infinity()).isValid());

        document.close();
        expect (! document.isOpen());
        expectEquals (document.getPageCount(), 0);
    }
};

static PdfCoreTests pdfCoreTests;

} // namespace ayra::tests
