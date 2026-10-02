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

// Implementazione Apple di MacPdfRenderer.
// CoreGraphics: lifecycle, metadata, raster.
// PDFKit: estrazione testo e ricerca.
// Roadmap canonica: _docs/02_stato_attuale.md.
// M01-M03 sono implementati staticamente; M04 completa il content model testuale.
// Il codice legacy e' solo riferimento storico e verra' rimosso al cutover current-only.

#if JUCE_MAC || JUCE_IOS

#include "../ayra_PdfSafetyLimits.h"

#import <PDFKit/PDFKit.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>

namespace ayra
{

namespace
{

[[nodiscard]] bool fitsFloat (CGFloat value) noexcept
{
    const auto d = static_cast<double> (value);
    constexpr auto maxFloat = static_cast<double> (std::numeric_limits<float>::max());
    return std::isfinite (d) && d >= -maxFloat && d <= maxFloat;
}

template <typename NativeRectangle>
[[nodiscard]] bool copyPdfBounds (const NativeRectangle& nativeBounds,
                                  juce::Rectangle<float>& destination) noexcept
{
    if (!fitsFloat (nativeBounds.origin.x) || !fitsFloat (nativeBounds.origin.y)
        || !fitsFloat (nativeBounds.size.width) || !fitsFloat (nativeBounds.size.height)
        || nativeBounds.size.width <= 0 || nativeBounds.size.height <= 0)
    {
        return false;
    }

    destination = { static_cast<float> (nativeBounds.origin.x),
                    static_cast<float> (nativeBounds.origin.y),
                    static_cast<float> (nativeBounds.size.width),
                    static_cast<float> (nativeBounds.size.height) };
    return !destination.isEmpty();
}

[[nodiscard]] bool copyNSStringToJuce (NSString* source, juce::String& destination)
{
    if (source == nil)
        return false;

    NSUInteger utf8Bytes = 0;
    const char* utf8 = nullptr;

    @try
    {
        utf8Bytes = [source lengthOfBytesUsingEncoding:NSUTF8StringEncoding];

        if (utf8Bytes > detail::maxTextUtf8BytesPerPage
            || utf8Bytes > static_cast<NSUInteger> (std::numeric_limits<int>::max()))
        {
            return false;
        }

        utf8 = [source UTF8String];
    }
    @catch (NSException*)
    {
        return false;
    }

    if (utf8 == nullptr)
        return false;

    destination = juce::String::fromUTF8 (utf8, static_cast<int> (utf8Bytes));
    return true;
}

class ProviderDataSnapshot final
{
public:
    explicit ProviderDataSnapshot (CGDataProviderRef provider) noexcept
        : data (provider != nullptr ? CGDataProviderCopyData (provider) : nullptr)
    {
        if (data == nullptr)
            return;

        const CFIndex length = CFDataGetLength (data);

        if (length <= 0
            || static_cast<std::uint64_t> (length) > detail::maxDocumentBytes)
        {
            return;
        }

        const auto* bytePtr = CFDataGetBytePtr (data);

        if (bytePtr == nullptr)
            return;

        bytes = bytePtr;
        size = static_cast<size_t> (length);
    }

    ~ProviderDataSnapshot()
    {
        if (data != nullptr)
            CFRelease (data);
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return bytes != nullptr && size > 0;
    }

    [[nodiscard]] const void* getData() const noexcept { return bytes; }
    [[nodiscard]] size_t getSize() const noexcept { return size; }

private:
    CFDataRef data { nullptr };
    const UInt8* bytes { nullptr };
    size_t size { 0 };

    ProviderDataSnapshot (const ProviderDataSnapshot&) = delete;
    ProviderDataSnapshot& operator= (const ProviderDataSnapshot&) = delete;
};

} // namespace

// ======================================================================
// Impl — stato ObjC nascosto agli header C++
// ======================================================================

struct MacPdfRenderer::Impl
{
    CGPDFDocumentRef document { nullptr };        ///< Documento CoreGraphics corrente
    CGDataProviderRef provider { nullptr };       ///< Provider mantenuto vivo quanto il documento
    std::unique_ptr<juce::MemoryBlock> pdfData;   ///< Backing storage per load-from-memory

    PDFDocument* textDocument { nil };             ///< Cache derivata PDFKit per text/search
    CFDataRef textData { nullptr };                ///< Byte snapshot posseduti dalla cache PDFKit

    [[nodiscard]] bool ensureTextDocument() noexcept
    {
        if (textDocument != nil)
            return true;

        if (provider == nullptr || document == nullptr)
            return false;

        CFDataRef newData = CGDataProviderCopyData (provider);

        if (newData == nullptr)
            return false;

        const CFIndex length = CFDataGetLength (newData);

        if (length <= 0
            || static_cast<std::uint64_t> (length) > detail::maxDocumentBytes)
        {
            CFRelease (newData);
            return false;
        }

        PDFDocument* newTextDocument = nil;

        @try
        {
            newTextDocument = [[PDFDocument alloc] initWithData:(__bridge NSData*) newData];

            if (newTextDocument != nil
                && [newTextDocument pageCount] != CGPDFDocumentGetNumberOfPages (document))
            {
                [newTextDocument release];
                newTextDocument = nil;
            }
        }
        @catch (NSException*)
        {
            if (newTextDocument != nil)
                [newTextDocument release];

            newTextDocument = nil;
        }

        if (newTextDocument == nil)
        {
            CFRelease (newData);
            return false;
        }

        textData = newData;
        textDocument = newTextDocument;
        return true;
    }

    void resetTextDocument() noexcept
    {
        if (textDocument != nil)
        {
            [textDocument release];
            textDocument = nil;
        }

        if (textData != nullptr)
        {
            CFRelease (textData);
            textData = nullptr;
        }
    }
};

// ======================================================================
// MacPdfRenderer
// ======================================================================

MacPdfRenderer::MacPdfRenderer()
    : impl (std::make_unique<Impl>()) {}

MacPdfRenderer::~MacPdfRenderer()
{
    close();
}

bool MacPdfRenderer::loadFromFile (const juce::File& file) noexcept
{
    if (!file.existsAsFile())
        return false;

    const auto fileSize = file.getSize();

    if (fileSize <= 0
        || static_cast<std::uint64_t> (fileSize) > detail::maxDocumentBytes)
    {
        return false;
    }

    const auto path = file.getFullPathName();
    CGDataProviderRef provider = CGDataProviderCreateWithFilename (path.toRawUTF8());

    if (provider == nullptr)
        return false;

    CGPDFDocumentRef newDocument = CGPDFDocumentCreateWithProvider (provider);

    if (newDocument == nullptr)
    {
        CGDataProviderRelease (provider);
        return false;
    }

    // Commit transazionale: un load fallito lascia intatto il documento corrente.
    close();
    impl->provider = provider;
    impl->document = newDocument;
    return true;
}

bool MacPdfRenderer::loadFromMemory (const void* data, size_t sizeBytes) noexcept
{
    if (data == nullptr || sizeBytes == 0
        || static_cast<std::uint64_t> (sizeBytes) > detail::maxDocumentBytes)
    {
        return false;
    }

    try
    {
        auto newPdfData = std::make_unique<juce::MemoryBlock>();
        newPdfData->replaceAll (data, sizeBytes);

        CGDataProviderRef provider = CGDataProviderCreateWithData (nullptr,
                                                                   newPdfData->getData(),
                                                                   newPdfData->getSize(),
                                                                   nullptr);

        if (provider == nullptr)
            return false;

        CGPDFDocumentRef newDocument = CGPDFDocumentCreateWithProvider (provider);

        if (newDocument == nullptr)
        {
            CGDataProviderRelease (provider);
            return false;
        }

        // Provider e backing storage restano entrambi owned dal renderer per tutta la vita
        // del documento. Nessun puntatore ai byte sopravvive al proprio owner.
        close();
        impl->pdfData = std::move (newPdfData);
        impl->provider = provider;
        impl->document = newDocument;
        return true;
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
}

bool MacPdfRenderer::saveToFile (const juce::File& destFile) const noexcept
{
    if (impl->document == nullptr || impl->provider == nullptr)
        return false;

    ProviderDataSnapshot snapshot (impl->provider);

    if (!snapshot.isValid())
        return false;

    try
    {
        // juce::File::replaceWithData writes through a temporary file before
        // replacing the destination, so a failed write does not truncate it.
        return destFile.replaceWithData (snapshot.getData(), snapshot.getSize());
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
}

bool MacPdfRenderer::saveToMemory (juce::MemoryBlock& destData) const noexcept
{
    if (impl->document == nullptr || impl->provider == nullptr)
        return false;

    ProviderDataSnapshot snapshot (impl->provider);

    if (!snapshot.isValid())
        return false;

    try
    {
        // Transactional destination semantics: build the replacement first, then
        // publish it only after the allocation/copy has completed successfully.
        juce::MemoryBlock replacement (snapshot.getData(), snapshot.getSize());
        destData = std::move (replacement);
        return true;
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
}

void MacPdfRenderer::close() noexcept
{
    impl->resetTextDocument();

    if (impl->document != nullptr)
    {
        CGPDFDocumentRelease (impl->document);
        impl->document = nullptr;
    }

    if (impl->provider != nullptr)
    {
        CGDataProviderRelease (impl->provider);
        impl->provider = nullptr;
    }

    impl->pdfData.reset();
}

bool MacPdfRenderer::isLoaded() const noexcept
{
    return impl->document != nullptr;
}

int MacPdfRenderer::getPageCount() const noexcept
{
    if (impl->document == nullptr)
        return 0;

    const auto count = CGPDFDocumentGetNumberOfPages (impl->document);
    constexpr auto maxInt = static_cast<size_t> (std::numeric_limits<int>::max());

    return count > maxInt ? std::numeric_limits<int>::max()
                          : static_cast<int> (count);
}

PdfPage MacPdfRenderer::getPage (int pageIndex) const noexcept
{
    if (impl->document == nullptr || pageIndex < 0)
        return {};

    const int addressablePageCount = getPageCount();

    if (pageIndex >= addressablePageCount)
        return {};

    const auto pageNumber = static_cast<size_t> (pageIndex) + 1u;

    CGPDFPageRef page = CGPDFDocumentGetPage (impl->document, pageNumber);

    if (page == nullptr)
        return {};

    const auto mediaBox = CGRectStandardize (CGPDFPageGetBoxRect (page, kCGPDFMediaBox));

    if (CGRectIsNull (mediaBox) || CGRectIsEmpty (mediaBox) || CGRectIsInfinite (mediaBox))
        return {};

    const int rawRotation = CGPDFPageGetRotationAngle (page);
    const int rotation = ((rawRotation % 360) + 360) % 360;

    if ((rotation % 90) != 0)
        return {};

    PdfPage result;
    result.index = pageIndex;

    if (!copyPdfBounds (mediaBox, result.bounds))
        return {};

    result.rotation = rotation;
    return result;
}

juce::Image MacPdfRenderer::renderPage (int pageIndex, float scale) noexcept
{
    if (!std::isfinite (scale) || scale <= 0.0f)
        return {};

    const auto pageInfo = getPage (pageIndex);

    if (!pageInfo.isValid())
        return {};

    const bool quarterTurn = pageInfo.rotation == 90 || pageInfo.rotation == 270;
    const double widthPoints = quarterTurn ? static_cast<double> (pageInfo.bounds.getHeight())
                                           : static_cast<double> (pageInfo.bounds.getWidth());
    const double heightPoints = quarterTurn ? static_cast<double> (pageInfo.bounds.getWidth())
                                            : static_cast<double> (pageInfo.bounds.getHeight());
    const double widthPixels = std::ceil (widthPoints * static_cast<double> (scale));
    const double heightPixels = std::ceil (heightPoints * static_cast<double> (scale));

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

    const auto pageNumber = static_cast<size_t> (pageIndex) + 1u;
    CGPDFPageRef page = CGPDFDocumentGetPage (impl->document, pageNumber);

    if (page == nullptr)
        return {};

    try
    {
        juce::SoftwareImageType imageType;
        juce::Image image (juce::Image::ARGB, width, height, true, imageType);

        if (!image.isValid())
            return {};

        juce::Image::BitmapData bitmap (image, juce::Image::BitmapData::writeOnly);

        if (bitmap.data == nullptr
            || bitmap.pixelFormat != juce::Image::ARGB
            || bitmap.pixelStride != static_cast<int> (sizeof (juce::PixelARGB))
            || bitmap.lineStride <= 0)
        {
            return {};
        }

        const auto requiredBytes = static_cast<std::uint64_t> (bitmap.lineStride)
                                 * static_cast<std::uint64_t> (height);

        if (requiredBytes > static_cast<std::uint64_t> (bitmap.size))
            return {};

        CGColorSpaceRef colourSpace = CGColorSpaceCreateDeviceRGB();

        if (colourSpace == nullptr)
            return {};

        const CGBitmapInfo bitmapInfo = static_cast<CGBitmapInfo> (
            kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);

        CGContextRef context = CGBitmapContextCreate (bitmap.data,
                                                      static_cast<size_t> (width),
                                                      static_cast<size_t> (height),
                                                      8,
                                                      static_cast<size_t> (bitmap.lineStride),
                                                      colourSpace,
                                                      bitmapInfo);
        CGColorSpaceRelease (colourSpace);

        if (context == nullptr)
            return {};

        // PDF pages have an implicit white backdrop. Keep the renderer output
        // deterministic and identical across CoreGraphics/PDFium backends.
        CGContextSetRGBFillColor (context, 1.0, 1.0, 1.0, 1.0);
        CGContextFillRect (context, CGRectMake (0.0, 0.0,
                                                static_cast<CGFloat> (width),
                                                static_cast<CGFloat> (height)));

        CGContextSaveGState (context);

        // Raw Quartz bitmap contexts are Y-up, while JUCE row 0 is the top line.
        // Flip once at the device boundary, then let CoreGraphics map PDF user
        // space (including /Rotate) into the destination rectangle.
        CGContextTranslateCTM (context, 0.0, static_cast<CGFloat> (height));
        CGContextScaleCTM (context, 1.0, -1.0);

        const CGRect destination = CGRectMake (0.0, 0.0,
                                               static_cast<CGFloat> (width),
                                               static_cast<CGFloat> (height));
        const auto transform = CGPDFPageGetDrawingTransform (page,
                                                             kCGPDFMediaBox,
                                                             destination,
                                                             0,
                                                             true);

        CGContextConcatCTM (context, transform);
        CGContextClipToRect (context, CGRectStandardize (CGPDFPageGetBoxRect (page, kCGPDFMediaBox)));
        CGContextDrawPDFPage (context, page);
        CGContextRestoreGState (context);
        CGContextRelease (context);

        return image;
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
}

juce::Array<PdfSearchResult> MacPdfRenderer::findText (const juce::String& query, int pageIndex) noexcept
{
    if (impl->document == nullptr || query.isEmpty() || pageIndex < -1)
        return {};

    const int pageCount = getPageCount();

    if (pageCount <= 0 || (pageIndex >= 0 && pageIndex >= pageCount))
        return {};

    try
    {
        const auto queryBytes = static_cast<std::uint64_t> (query.getNumBytesAsUTF8());

        if (queryBytes == 0 || queryBytes > detail::maxSearchQueryUtf8Bytes)
            return {};

        if (!impl->ensureTextDocument())
            return {};

        juce::Array<PdfSearchResult> results;

        @autoreleasepool
        {
            NSString* needle = nil;

            @try
            {
                needle = [NSString stringWithUTF8String:query.toRawUTF8()];
            }
            @catch (NSException*)
            {
                return {};
            }

            if (needle == nil)
                return {};

            const int firstPage = pageIndex >= 0 ? pageIndex : 0;
            const int lastPage = pageIndex >= 0 ? pageIndex : pageCount - 1;

            for (int currentPage = firstPage; currentPage <= lastPage; ++currentPage)
            {
                PDFPage* page = nil;
                NSString* pageText = nil;

                @try
                {
                    page = [impl->textDocument pageAtIndex:static_cast<NSUInteger> (currentPage)];
                    pageText = [page string];
                }
                @catch (NSException*)
                {
                    return {};
                }

                if (page == nil || pageText == nil || [pageText length] == 0)
                    continue;

                NSUInteger pageUtf8Bytes = 0;

                @try
                {
                    pageUtf8Bytes = [pageText lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
                }
                @catch (NSException*)
                {
                    return {};
                }

                if (pageUtf8Bytes > detail::maxTextUtf8BytesPerPage)
                    return {};

                const NSUInteger textLength = [pageText length];
                NSRange remaining = NSMakeRange (0, textLength);

                while (remaining.length > 0
                       && results.size() < detail::maxSearchResults)
                {
                    NSRange match = NSMakeRange (NSNotFound, 0);

                    @try
                    {
                        match = [pageText rangeOfString:needle
                                               options:NSCaseInsensitiveSearch
                                                 range:remaining];
                    }
                    @catch (NSException*)
                    {
                        return {};
                    }

                    if (match.location == NSNotFound || match.length == 0)
                        break;

                    PDFSelection* selection = nil;
                    juce::Rectangle<float> bounds;
                    NSString* matchedText = nil;

                    @try
                    {
                        selection = [page selectionForRange:match];

                        if (selection != nil)
                        {
                            const auto nativeBounds = [selection boundsForPage:page];
                            copyPdfBounds (nativeBounds, bounds);
                            matchedText = [pageText substringWithRange:match];
                        }
                    }
                    @catch (NSException*)
                    {
                        return {};
                    }

                    if (selection != nil && !bounds.isEmpty() && matchedText != nil)
                    {
                        PdfSearchResult result;
                        result.pageIndex = currentPage;
                        result.bounds = bounds;

                        if (!copyNSStringToJuce (matchedText, result.text) || result.text.isEmpty())
                            return {};

                        results.add (std::move (result));
                    }

                    const NSUInteger next = NSMaxRange (match);

                    if (next <= match.location || next >= textLength)
                        break;

                    remaining = NSMakeRange (next, textLength - next);
                }

                if (results.size() >= detail::maxSearchResults)
                    break;
            }
        }

        return results;
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
}

juce::String MacPdfRenderer::extractText (int pageIndex) noexcept
{
    if (impl->document == nullptr || pageIndex < 0 || pageIndex >= getPageCount())
        return {};

    if (!impl->ensureTextDocument())
        return {};

    try
    {
        @autoreleasepool
        {
            NSString* pageText = nil;

            @try
            {
                PDFPage* page = [impl->textDocument pageAtIndex:static_cast<NSUInteger> (pageIndex)];
                pageText = [page string];
            }
            @catch (NSException*)
            {
                return {};
            }

            juce::String result;

            if (!copyNSStringToJuce (pageText, result))
                return {};

            return result;
        }
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
}

} // namespace ayra

#endif // JUCE_MAC || JUCE_IOS
