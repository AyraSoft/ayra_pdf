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

// Backend PDFium per Windows, Linux e Android.
// Tutte le API FPDF sono serializzate process-wide per contratto PDFium.

#if JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID

#include "../ayra_PdfByteSource.h"

#include <utility>

namespace ayra
{

namespace
{

struct PdfiumProcessState final
{
    juce::CriticalSection apiLock;
    int rendererCount { 0 };
};

[[nodiscard]] PdfiumProcessState& getPdfiumProcessState() noexcept
{
    static PdfiumProcessState state;
    return state;
}

void retainPdfiumLibrary() noexcept
{
    auto& state = getPdfiumProcessState();
    const juce::ScopedLock lock (state.apiLock);

    if (state.rendererCount == 0)
    {
        FPDF_LIBRARY_CONFIG config {};
        config.version = 2;
        config.m_pUserFontPaths = nullptr;
        config.m_pIsolate = nullptr;
        config.m_v8EmbedderSlot = 0;
        FPDF_InitLibraryWithConfig (&config);
    }

    ++state.rendererCount;
}

void releasePdfiumLibrary() noexcept
{
    auto& state = getPdfiumProcessState();
    const juce::ScopedLock lock (state.apiLock);

    if (state.rendererCount <= 0)
        return;

    --state.rendererCount;

    if (state.rendererCount == 0)
        FPDF_DestroyLibrary();
}

[[nodiscard]] bool fitsFloat (double value) noexcept
{
    constexpr auto maxFloat = static_cast<double> (std::numeric_limits<float>::max());
    return std::isfinite (value) && value >= -maxFloat && value <= maxFloat;
}

class ScopedPdfiumPage final
{
public:
    explicit ScopedPdfiumPage (FPDF_PAGE pageIn) noexcept : page (pageIn) {}
    ~ScopedPdfiumPage()
    {
        if (page != nullptr)
            FPDF_ClosePage (page);
    }

    [[nodiscard]] FPDF_PAGE get() const noexcept { return page; }

private:
    FPDF_PAGE page { nullptr };
};

class ScopedPdfiumBitmap final
{
public:
    explicit ScopedPdfiumBitmap (FPDF_BITMAP bitmapIn) noexcept : bitmap (bitmapIn) {}
    ~ScopedPdfiumBitmap()
    {
        if (bitmap != nullptr)
            FPDFBitmap_Destroy (bitmap);
    }

    [[nodiscard]] FPDF_BITMAP get() const noexcept { return bitmap; }

private:
    FPDF_BITMAP bitmap { nullptr };
};

class ScopedPdfiumTextPage final
{
public:
    explicit ScopedPdfiumTextPage (FPDF_TEXTPAGE textPageIn) noexcept : textPage (textPageIn) {}
    ~ScopedPdfiumTextPage()
    {
        if (textPage != nullptr)
            FPDFText_ClosePage (textPage);
    }

    [[nodiscard]] FPDF_TEXTPAGE get() const noexcept { return textPage; }

private:
    FPDF_TEXTPAGE textPage { nullptr };
};

class ScopedPdfiumSearch final
{
public:
    explicit ScopedPdfiumSearch (FPDF_SCHHANDLE handleIn) noexcept : handle (handleIn) {}
    ~ScopedPdfiumSearch()
    {
        if (handle != nullptr)
            FPDFText_FindClose (handle);
    }

    [[nodiscard]] FPDF_SCHHANDLE get() const noexcept { return handle; }

private:
    FPDF_SCHHANDLE handle { nullptr };
};

[[nodiscard]] constexpr FPDF_WCHAR byteSwap16 (FPDF_WCHAR value) noexcept
{
    return static_cast<FPDF_WCHAR> ((value >> 8) | (value << 8));
}

[[nodiscard]] bool makePdfiumWideString (const juce::String& source,
                                         std::vector<FPDF_WCHAR>& destination)
{
    using JuceUtf16Unit = juce::CharPointer_UTF16::CharType;
    static_assert (sizeof (FPDF_WCHAR) == sizeof (JuceUtf16Unit));

    const auto sourceUtf8Bytes = static_cast<std::uint64_t> (source.getNumBytesAsUTF8());

    if (source.isEmpty() || sourceUtf8Bytes == 0
        || sourceUtf8Bytes > detail::maxSearchQueryUtf8Bytes)
    {
        return false;
    }

    const size_t capacityUnits = static_cast<size_t> (source.length()) * 2u + 1u;
    std::vector<JuceUtf16Unit> juceUnits (capacityUnits, 0);

    const auto bytesWritten = source.copyToUTF16 (juceUnits.data(),
                                                  juceUnits.size() * sizeof (JuceUtf16Unit));

    if (bytesWritten < sizeof (JuceUtf16Unit)
        || (bytesWritten % sizeof (JuceUtf16Unit)) != 0
        || bytesWritten > juceUnits.size() * sizeof (JuceUtf16Unit))
    {
        return false;
    }

    const size_t unitsWritten = bytesWritten / sizeof (JuceUtf16Unit);
    destination.assign (unitsWritten, 0);

    for (size_t i = 0; i < unitsWritten; ++i)
    {
        FPDF_WCHAR unit = static_cast<FPDF_WCHAR> (
            static_cast<std::uint16_t> (juceUnits[i]));

       #if JUCE_BIG_ENDIAN
        unit = byteSwap16 (unit);
       #endif

        destination[i] = unit;
    }

    return !destination.empty() && destination.front() != 0 && destination.back() == 0;
}

[[nodiscard]] bool textRangeToJuceString (FPDF_TEXTPAGE textPage,
                                          int startIndex,
                                          int count,
                                          juce::String& destination)
{
    using JuceUtf16Unit = juce::CharPointer_UTF16::CharType;
    static_assert (sizeof (FPDF_WCHAR) == sizeof (JuceUtf16Unit));

    if (textPage == nullptr || startIndex < 0 || count <= 0
        || count > detail::maxTextCodeUnitsPerPage)
    {
        return false;
    }

    std::vector<FPDF_WCHAR> pdfiumUnits (static_cast<size_t> (count) + 1u, 0);
    const int written = FPDFText_GetText (textPage,
                                          startIndex,
                                          count,
                                          pdfiumUnits.data());

    if (written <= 0 || written > static_cast<int> (pdfiumUnits.size()))
        return false;

    std::vector<JuceUtf16Unit> juceUnits (static_cast<size_t> (written), 0);

    for (int i = 0; i < written - 1; ++i)
    {
        FPDF_WCHAR unit = pdfiumUnits[static_cast<size_t> (i)];

       #if JUCE_BIG_ENDIAN
        unit = byteSwap16 (unit);
       #endif

        juceUnits[static_cast<size_t> (i)] = static_cast<JuceUtf16Unit> (unit);
    }

    juceUnits.back() = 0;
    destination = juce::String (juce::CharPointer_UTF16 (juceUnits.data()));

    return static_cast<std::uint64_t> (destination.getNumBytesAsUTF8())
        <= detail::maxTextUtf8BytesPerPage;
}

} // namespace

// ======================================================================
// Impl — documento/backing bytes; tutte le chiamate FPDF richiedono apiLock.
// ======================================================================

struct PdfiumRenderer::Impl
{
    FPDF_DOCUMENT document { nullptr };
    std::shared_ptr<const juce::MemoryBlock> pdfData;

    [[nodiscard]] std::shared_ptr<const juce::MemoryBlock>
    closeUnlockedAndRetireBytes() noexcept
    {
        if (document != nullptr)
        {
            FPDF_CloseDocument (document);
            document = nullptr;
        }

        return std::exchange (pdfData, {});
    }

    [[nodiscard]] bool loadOwnedData (std::shared_ptr<const juce::MemoryBlock> newData) noexcept
    {
        if (newData == nullptr || newData->getSize() == 0)
            return false;

        std::shared_ptr<const juce::MemoryBlock> retiredBytes;

        {
            auto& state = getPdfiumProcessState();
            const juce::ScopedLock lock (state.apiLock);

            FPDF_DOCUMENT newDocument = FPDF_LoadMemDocument64 (newData->getData(),
                                                                newData->getSize(),
                                                                nullptr);

            if (newDocument == nullptr)
                return false;

            retiredBytes = closeUnlockedAndRetireBytes();
            pdfData = std::move (newData);
            document = newDocument;
        }

        // Potentially large MemoryBlock reclamation happens after releasing the
        // process-wide PDFium lock.
        retiredBytes.reset();
        return true;
    }
};

// ======================================================================
// PdfiumRenderer
// ======================================================================

PdfiumRenderer::PdfiumRenderer()
    : impl (std::make_unique<Impl>())
{
    retainPdfiumLibrary();
}

PdfiumRenderer::~PdfiumRenderer()
{
    close();
    releasePdfiumLibrary();
}

bool PdfiumRenderer::loadFromFile (const juce::File& file) noexcept
{
    const auto newData = detail::readOwnedPdfFileBytes (file);
    return newData != nullptr && impl->loadOwnedData (newData);
}

bool PdfiumRenderer::loadFromMemory (const void* data, size_t sizeBytes) noexcept
{
    if (data == nullptr || sizeBytes == 0
        || static_cast<std::uint64_t> (sizeBytes) > detail::maxDocumentBytes)
    {
        return false;
    }

    try
    {
        auto mutableData = std::make_shared<juce::MemoryBlock> (data, sizeBytes);
        std::shared_ptr<const juce::MemoryBlock> newData = std::move (mutableData);
        return impl->loadOwnedData (std::move (newData));
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
}

bool PdfiumRenderer::saveToFile (const juce::File& destFile) const noexcept
{
    std::shared_ptr<const juce::MemoryBlock> snapshot;

    {
        auto& state = getPdfiumProcessState();
        const juce::ScopedLock lock (state.apiLock);

        if (impl->document == nullptr || impl->pdfData == nullptr
            || impl->pdfData->getSize() == 0)
        {
            return false;
        }

        snapshot = impl->pdfData;
    }

    try
    {
        return destFile.replaceWithData (
            snapshot->getData(),
            snapshot->getSize());
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
}

bool PdfiumRenderer::saveToMemory (juce::MemoryBlock& destData) const noexcept
{
    std::shared_ptr<const juce::MemoryBlock> snapshot;

    {
        auto& state = getPdfiumProcessState();
        const juce::ScopedLock lock (state.apiLock);

        if (impl->document == nullptr || impl->pdfData == nullptr
            || impl->pdfData->getSize() == 0)
        {
            return false;
        }

        snapshot = impl->pdfData;
    }

    try
    {
        juce::MemoryBlock replacement (
            snapshot->getData(),
            snapshot->getSize());
        destData = std::move (replacement);
        return true;
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
}

bool PdfiumRenderer::sourceBytesEqualFile (const juce::File& file) const noexcept
{
    std::shared_ptr<const juce::MemoryBlock> snapshot;

    {
        auto& state = getPdfiumProcessState();
        const juce::ScopedLock lock (state.apiLock);

        if (impl->document == nullptr || impl->pdfData == nullptr)
            return false;

        snapshot = impl->pdfData;
    }

    return detail::sourceBytesEqualFile (
        file,
        snapshot->getData(),
        snapshot->getSize());
}

void PdfiumRenderer::close() noexcept
{
    std::shared_ptr<const juce::MemoryBlock> retiredBytes;

    {
        auto& state = getPdfiumProcessState();
        const juce::ScopedLock lock (state.apiLock);
        retiredBytes = impl->closeUnlockedAndRetireBytes();
    }

    retiredBytes.reset();
}

bool PdfiumRenderer::isLoaded() const noexcept
{
    auto& state = getPdfiumProcessState();
    const juce::ScopedLock lock (state.apiLock);
    return impl->document != nullptr;
}

int PdfiumRenderer::getPageCount() const noexcept
{
    auto& state = getPdfiumProcessState();
    const juce::ScopedLock lock (state.apiLock);

    if (impl->document == nullptr)
        return 0;

    const int count = FPDF_GetPageCount (impl->document);
    return count > 0 ? count : 0;
}

PdfPage PdfiumRenderer::getPage (int pageIndex) const noexcept
{
    if (pageIndex < 0)
        return {};

    auto& state = getPdfiumProcessState();
    const juce::ScopedLock lock (state.apiLock);

    if (impl->document == nullptr)
        return {};

    const int pageCount = FPDF_GetPageCount (impl->document);

    if (pageCount <= 0 || pageIndex >= pageCount)
        return {};

    FPDF_PAGE page = FPDF_LoadPage (impl->document, pageIndex);

    if (page == nullptr)
        return {};

    FS_RECTF visibleBox {};
    const bool hasVisibleBox = FPDF_GetPageBoundingBox (page, &visibleBox) != 0;
    const int rotationQuarterTurns = FPDFPage_GetRotation (page);

    FPDF_ClosePage (page);

    if (!hasVisibleBox || rotationQuarterTurns < 0 || rotationQuarterTurns > 3)
        return {};

    const double left = static_cast<double> (visibleBox.left);
    const double bottom = static_cast<double> (visibleBox.bottom);
    const double right = static_cast<double> (visibleBox.right);
    const double top = static_cast<double> (visibleBox.top);
    const double width = right - left;
    const double height = top - bottom;

    if (!fitsFloat (left) || !fitsFloat (bottom)
        || !fitsFloat (width) || !fitsFloat (height)
        || width <= 0.0 || height <= 0.0)
    {
        return {};
    }

    PdfPage result;
    result.index = pageIndex;
    result.bounds = { static_cast<float> (left),
                      static_cast<float> (bottom),
                      static_cast<float> (width),
                      static_cast<float> (height) };
    result.rotation = rotationQuarterTurns * 90;
    return result;
}

juce::Image PdfiumRenderer::renderPage (int pageIndex, float scale) noexcept
{
    if (pageIndex < 0 || !std::isfinite (scale) || scale <= 0.0f)
        return {};

    try
    {
        auto& state = getPdfiumProcessState();
        const juce::ScopedLock lock (state.apiLock);

        if (impl->document == nullptr)
            return {};

        const int pageCount = FPDF_GetPageCount (impl->document);

        if (pageCount <= 0 || pageIndex >= pageCount)
            return {};

        ScopedPdfiumPage page (FPDF_LoadPage (impl->document, pageIndex));

        if (page.get() == nullptr)
            return {};

        const double widthPixels = std::ceil (
            static_cast<double> (FPDF_GetPageWidthF (page.get())) * static_cast<double> (scale));
        const double heightPixels = std::ceil (
            static_cast<double> (FPDF_GetPageHeightF (page.get())) * static_cast<double> (scale));

        if (!std::isfinite (widthPixels) || !std::isfinite (heightPixels)
            || widthPixels <= 0.0 || heightPixels <= 0.0
            || widthPixels > static_cast<double> (detail::maxRasterDimension)
            || heightPixels > static_cast<double> (detail::maxRasterDimension))
        {
            return {};
        }

        const int width = static_cast<int> (widthPixels);
        const int height = static_cast<int> (heightPixels);
        const auto pixelCount = static_cast<std::uint64_t> (width)
                              * static_cast<std::uint64_t> (height);

        if (pixelCount > detail::maxRasterPixels)
            return {};

        juce::SoftwareImageType imageType;
        juce::Image image (juce::Image::ARGB, width, height, true, imageType);

        if (!image.isValid())
            return {};

        juce::Image::BitmapData bitmapData (image, juce::Image::BitmapData::writeOnly);

        if (bitmapData.data == nullptr
            || bitmapData.pixelFormat != juce::Image::ARGB
            || bitmapData.pixelStride != static_cast<int> (sizeof (juce::PixelARGB))
            || bitmapData.lineStride <= 0)
        {
            return {};
        }

        const auto requiredBytes = static_cast<std::uint64_t> (bitmapData.lineStride)
                                 * static_cast<std::uint64_t> (height);

        if (requiredBytes > static_cast<std::uint64_t> (bitmapData.size))
            return {};

        ScopedPdfiumBitmap bitmap (
            FPDFBitmap_CreateEx (width,
                                 height,
                                 FPDFBitmap_BGRA,
                                 bitmapData.data,
                                 bitmapData.lineStride));

        if (bitmap.get() == nullptr)
            return {};

        FPDFBitmap_FillRect (bitmap.get(), 0, 0, width, height, 0xffffffffu);

        int renderFlags = FPDF_RENDER_LIMITEDIMAGECACHE;

       #if JUCE_ANDROID
        // juce::PixelARGB is RGBA in memory on Android; PDFium BGRA rendering
        // needs RGB byte order there. Windows/Linux native PixelARGB is BGRA.
        renderFlags |= FPDF_REVERSE_BYTE_ORDER;
       #endif

        FPDF_RenderPageBitmap (bitmap.get(),
                               page.get(),
                               0,
                               0,
                               width,
                               height,
                               0,
                               renderFlags);

        return image;
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
}

juce::Array<PdfSearchResult> PdfiumRenderer::findText (const juce::String& query, int pageIndex) noexcept
{
    if (query.isEmpty() || pageIndex < -1)
        return {};

    try
    {
        std::vector<FPDF_WCHAR> queryUtf16;

        if (!makePdfiumWideString (query, queryUtf16))
            return {};

        juce::Array<PdfSearchResult> results;
        int examinedMatches = 0;

        auto& state = getPdfiumProcessState();
        const juce::ScopedLock lock (state.apiLock);

        if (impl->document == nullptr)
            return {};

        const int pageCount = FPDF_GetPageCount (impl->document);

        if (pageCount <= 0 || (pageIndex >= 0 && pageIndex >= pageCount))
            return {};

        const int firstPage = pageIndex >= 0 ? pageIndex : 0;
        const int lastPage = pageIndex >= 0 ? pageIndex : pageCount - 1;

        for (int currentPage = firstPage; currentPage <= lastPage; ++currentPage)
        {
            ScopedPdfiumPage page (FPDF_LoadPage (impl->document, currentPage));

            if (page.get() == nullptr)
                return {};

            ScopedPdfiumTextPage textPage (FPDFText_LoadPage (page.get()));

            if (textPage.get() == nullptr)
                return {};

            const int charCount = FPDFText_CountChars (textPage.get());

            if (charCount < 0 || charCount > detail::maxTextCodeUnitsPerPage)
                return {};

            if (charCount == 0)
                continue;

            ScopedPdfiumSearch search (
                FPDFText_FindStart (textPage.get(),
                                    queryUtf16.data(),
                                    0,
                                    0));

            if (search.get() == nullptr)
                return {};

            while (FPDFText_FindNext (search.get()) != 0)
            {
                if (++examinedMatches > detail::maxSearchResults)
                    return {};

                const int matchIndex = FPDFText_GetSchResultIndex (search.get());
                const int matchCount = FPDFText_GetSchCount (search.get());

                if (matchIndex < 0 || matchCount <= 0
                    || matchIndex > charCount
                    || matchCount > charCount - matchIndex)
                {
                    return {};
                }

                const int rectCount = FPDFText_CountRects (textPage.get(),
                                                          matchIndex,
                                                          matchCount);

                if (rectCount < 0)
                    return {};

                if (rectCount == 0)
                    continue;

                double unionLeft = std::numeric_limits<double>::infinity();
                double unionBottom = std::numeric_limits<double>::infinity();
                double unionRight = -std::numeric_limits<double>::infinity();
                double unionTop = -std::numeric_limits<double>::infinity();

                for (int rectIndex = 0; rectIndex < rectCount; ++rectIndex)
                {
                    double left = 0.0;
                    double top = 0.0;
                    double right = 0.0;
                    double bottom = 0.0;

                    if (FPDFText_GetRect (textPage.get(),
                                          rectIndex,
                                          &left,
                                          &top,
                                          &right,
                                          &bottom) == 0)
                    {
                        return {};
                    }

                    if (!std::isfinite (left) || !std::isfinite (right)
                        || !std::isfinite (bottom) || !std::isfinite (top)
                        || right <= left || top <= bottom)
                    {
                        return {};
                    }

                    unionLeft = std::min (unionLeft, left);
                    unionBottom = std::min (unionBottom, bottom);
                    unionRight = std::max (unionRight, right);
                    unionTop = std::max (unionTop, top);
                }

                const double width = unionRight - unionLeft;
                const double height = unionTop - unionBottom;

                if (!fitsFloat (unionLeft) || !fitsFloat (unionBottom)
                    || !fitsFloat (width) || !fitsFloat (height)
                    || width <= 0.0 || height <= 0.0)
                {
                    return {};
                }

                PdfSearchResult result;
                result.pageIndex = currentPage;
                result.bounds = { static_cast<float> (unionLeft),
                                  static_cast<float> (unionBottom),
                                  static_cast<float> (width),
                                  static_cast<float> (height) };

                if (!textRangeToJuceString (textPage.get(),
                                            matchIndex,
                                            matchCount,
                                            result.text)
                    || result.text.isEmpty())
                {
                    return {};
                }

                results.add (std::move (result));
            }
        }

        return results;
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
}

juce::String PdfiumRenderer::extractText (int pageIndex) noexcept
{
    if (pageIndex < 0)
        return {};

    try
    {
        auto& state = getPdfiumProcessState();
        const juce::ScopedLock lock (state.apiLock);

        if (impl->document == nullptr)
            return {};

        const int pageCount = FPDF_GetPageCount (impl->document);

        if (pageCount <= 0 || pageIndex >= pageCount)
            return {};

        ScopedPdfiumPage page (FPDF_LoadPage (impl->document, pageIndex));

        if (page.get() == nullptr)
            return {};

        ScopedPdfiumTextPage textPage (FPDFText_LoadPage (page.get()));

        if (textPage.get() == nullptr)
            return {};

        const int charCount = FPDFText_CountChars (textPage.get());

        if (charCount < 0 || charCount > detail::maxTextCodeUnitsPerPage)
            return {};

        if (charCount == 0)
            return {};

        juce::String result;

        if (!textRangeToJuceString (textPage.get(), 0, charCount, result))
            return {};

        return result;
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
}

} // namespace ayra

#endif // JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID
