/*
     ______  __    __  ______   ______
    |      \\|  \\  |  \\/      \\ |      \\
     \\######\\ ##  | ##  ######\\ \\######\\
     /      ## ##  | ## ##   \\##/      ##   Copyright 2023-2026
    |  #######\\ ##__/ ## ##     |  #######   Ayra Soft
     \\##    ##\\##    ## ##      \\##    ##   www.ayra.live
      \\#######_\\######\\##       \\#######
             |  \\__| ##
              \\##    ##
                \\######

 Ayra uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "ayra_PdfSafetyLimits.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <new>

namespace ayra::detail
{

[[nodiscard]] inline std::shared_ptr<const juce::MemoryBlock>
readOwnedPdfFileBytes (const juce::File& file) noexcept
{
    try
    {
        if (!file.existsAsFile())
            return {};

        auto input = file.createInputStream();

        if (input == nullptr)
            return {};

        const auto length = input->getTotalLength();

        if (length <= 0
            || static_cast<std::uint64_t> (length) > maxDocumentBytes)
        {
            return {};
        }

        auto data = std::make_shared<juce::MemoryBlock> (
            static_cast<size_t> (length),
            false);

        auto* destination = static_cast<char*> (data->getData());
        size_t offset = 0;
        size_t remaining = static_cast<size_t> (length);

        while (remaining > 0)
        {
            constexpr size_t maxChunk = 1024u * 1024u;
            const int requested = static_cast<int> (
                std::min (remaining, maxChunk));
            const int bytesRead = input->read (
                destination + offset,
                requested);

            if (bytesRead <= 0 || bytesRead > requested)
                return {};

            offset += static_cast<size_t> (bytesRead);
            remaining -= static_cast<size_t> (bytesRead);
        }

        return data;
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
}

[[nodiscard]] inline bool sourceBytesEqualFile (
    const juce::File& file,
    const void* sourceData,
    size_t sourceSize) noexcept
{
    try
    {
        if (sourceData == nullptr || sourceSize == 0
            || !file.existsAsFile()
            || file.getSize() != static_cast<juce::int64> (sourceSize))
        {
            return false;
        }

        auto input = file.createInputStream();

        if (input == nullptr)
            return false;

        std::array<char, 64u * 1024u> chunk {};
        const auto* expected = static_cast<const char*> (sourceData);
        size_t offset = 0;

        while (offset < sourceSize)
        {
            const int requested = static_cast<int> (
                std::min (sourceSize - offset, chunk.size()));
            const int bytesRead = input->read (chunk.data(), requested);

            if (bytesRead <= 0 || bytesRead > requested
                || std::memcmp (chunk.data(),
                                expected + offset,
                                static_cast<size_t> (bytesRead)) != 0)
            {
                return false;
            }

            offset += static_cast<size_t> (bytesRead);
        }

        return true;
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
}

} // namespace ayra::detail
