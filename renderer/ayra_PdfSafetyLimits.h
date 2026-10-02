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

/** @file
 *  Limiti di sicurezza canonici per input, raster e testo di tutti i backend ayra_pdf.
 *  Sono resource guard del modulo, non limiti del formato PDF.
 */

#pragma once


namespace ayra::detail
{

// Canonical safety limits shared by every ayra_pdf backend.
// These are resource guards, not PDF format limits.
inline constexpr std::uint64_t maxDocumentBytes = 512ull * 1024ull * 1024ull;
inline constexpr int maxRasterDimension = 16384;
inline constexpr std::uint64_t maxRasterPixels = 64ull * 1024ull * 1024ull;
inline constexpr std::uint64_t maxTextUtf8BytesPerPage = 64ull * 1024ull * 1024ull;
inline constexpr int maxTextCodeUnitsPerPage = 16 * 1024 * 1024;
inline constexpr std::uint64_t maxSearchQueryUtf8Bytes = 64ull * 1024ull;
inline constexpr int maxSearchResults = 100000;

} // namespace ayra::detail
