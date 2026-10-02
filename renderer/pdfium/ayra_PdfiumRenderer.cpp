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

// Backend PDFium current-only.
// P01: lifecycle process-wide, load owned-memory, close e metadata.
// P02/P03 completeranno save/render e text/search.

#if JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID

#include "../ayra_PdfSafetyLimits.h"

#include "../../third_party/pdfium/include/fpdfview.h"
#include "../../third_party/pdfium/include/fpdf_edit.h"
#include "../../third_party/pdfium/include/fpdf_transformpage.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>

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

} // namespace

// ======================================================================
// Impl — documento/backing bytes; tutte le chiamate FPDF richiedono apiLock.
// ======================================================================

struct PdfiumRenderer::Impl
{
    FPDF_DOCUMENT document { nullptr };
    std::unique_ptr<juce::MemoryBlock> pdfData;

    void closeUnlocked() noexcept
    {
        if (document != nullptr)
        {
            FPDF_CloseDocument (document);
            document = nullptr;
        }

        pdfData.reset();
    }

    [[nodiscard]] bool loadOwnedData (std::unique_ptr<juce::MemoryBlock> newData) noexcept
    {
        if (newData == nullptr || newData->getSize() == 0)
            return false;

        auto& state = getPdfiumProcessState();
        const juce::ScopedLock lock (state.apiLock);

        FPDF_DOCUMENT newDocument = FPDF_LoadMemDocument64 (newData->getData(),
                                                            newData->getSize(),
                                                            nullptr);

        if (newDocument == nullptr)
            return false;

        closeUnlocked();
        pdfData = std::move (newData);
        document = newDocument;
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
    try
    {
        auto input = file.createInputStream();

        if (input == nullptr)
            return false;

        const auto length = input->getTotalLength();

        if (length <= 0
            || static_cast<std::uint64_t> (length) > detail::maxDocumentBytes
            || length > static_cast<juce::int64> (std::numeric_limits<int>::max()))
        {
            return false;
        }

        auto newData = std::make_unique<juce::MemoryBlock>();
        newData->setSize (static_cast<size_t> (length), false);

        auto* destination = static_cast<std::uint8_t*> (newData->getData());
        int bytesRemaining = static_cast<int> (length);
        size_t offset = 0;

        while (bytesRemaining > 0)
        {
            constexpr int maxReadChunk = 1024 * 1024;
            const int requested = std::min (bytesRemaining, maxReadChunk);
            const int bytesRead = input->read (destination + offset, requested);

            if (bytesRead <= 0 || bytesRead > requested)
                return false;

            offset += static_cast<size_t> (bytesRead);
            bytesRemaining -= bytesRead;
        }

        return impl->loadOwnedData (std::move (newData));
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
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
        auto newData = std::make_unique<juce::MemoryBlock>();
        newData->replaceAll (data, sizeBytes);
        return impl->loadOwnedData (std::move (newData));
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
}

bool PdfiumRenderer::saveToFile (const juce::File& destFile) const noexcept
{
    juce::ignoreUnused (destFile);
    jassertfalse; // P02
    return false;
}

bool PdfiumRenderer::saveToMemory (juce::MemoryBlock& destData) const noexcept
{
    juce::ignoreUnused (destData);
    jassertfalse; // P02
    return false;
}

void PdfiumRenderer::close() noexcept
{
    auto& state = getPdfiumProcessState();
    const juce::ScopedLock lock (state.apiLock);
    impl->closeUnlocked();
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

    float left = 0.0f;
    float bottom = 0.0f;
    float right = 0.0f;
    float top = 0.0f;

    const bool hasMediaBox = FPDFPage_GetMediaBox (page, &left, &bottom, &right, &top) != 0;
    const int rotationQuarterTurns = FPDFPage_GetRotation (page);

    FPDF_ClosePage (page);

    if (!hasMediaBox || rotationQuarterTurns < 0 || rotationQuarterTurns > 3)
        return {};

    const double width = static_cast<double> (right) - static_cast<double> (left);
    const double height = static_cast<double> (top) - static_cast<double> (bottom);

    if (!fitsFloat (left) || !fitsFloat (bottom)
        || !fitsFloat (width) || !fitsFloat (height)
        || width <= 0.0 || height <= 0.0)
    {
        return {};
    }

    PdfPage result;
    result.index = pageIndex;
    result.bounds = { left,
                      bottom,
                      static_cast<float> (width),
                      static_cast<float> (height) };
    result.rotation = rotationQuarterTurns * 90;
    return result;
}

juce::Image PdfiumRenderer::renderPage (int pageIndex, float scale) noexcept
{
    juce::ignoreUnused (pageIndex, scale);
    jassertfalse; // P02
    return {};
}

juce::Array<PdfSearchResult> PdfiumRenderer::findText (const juce::String& query, int pageIndex) noexcept
{
    juce::ignoreUnused (query, pageIndex);
    jassertfalse; // P03
    return {};
}

juce::String PdfiumRenderer::extractText (int pageIndex) noexcept
{
    juce::ignoreUnused (pageIndex);
    jassertfalse; // P03
    return {};
}

} // namespace ayra

#endif // JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID
