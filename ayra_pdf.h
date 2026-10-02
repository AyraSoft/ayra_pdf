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

/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                   ayra_pdf
  vendor:               ayra.productions
  version:              2.0
  name:                 AYRA PDF
  description:          Rendering, ricerca, estrazione testo e visualizzazione PDF cross-platform.
                        macOS/iOS usa CoreGraphics + PDFKit. Windows/Linux/Android usa PDFium (Apache 2.0).
  website:
  license:              Copyright. All Rights Reserved.

  dependencies:         juce_graphics
  OSXFrameworks:        CoreGraphics PDFKit
  iOSFrameworks:        CoreGraphics PDFKit

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#define JUCE_AYRA_PDF_H_INCLUDED

#include <juce_graphics/juce_graphics.h>

#ifndef AYRA_PDF_HEADLESS
  #if ! defined (JUCE_MODULE_AVAILABLE_juce_gui_basics) || ! JUCE_MODULE_AVAILABLE_juce_gui_basics
    #error "ayra_pdf GUI requires juce_gui_basics. Add it to the consumer target or define AYRA_PDF_HEADLESS."
  #endif

  #include <juce_gui_basics/juce_gui_basics.h>
#endif

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

//==============================================================================
// ENGINE -- logica pura, headless-safe (nessuna dipendenza GUI)
#include "engine/ayra_PdfPage.h"
#include "engine/ayra_PdfSearchResult.h"
#include "engine/ayra_PdfDocument.h"

//==============================================================================
// RENDERER -- interfaccia pura + header delle implementazioni platform-specific
#include "renderer/ayra_PdfRenderer.h"

#if JUCE_MAC || JUCE_IOS
  #include "renderer/mac/ayra_MacPdfRenderer.h"
#elif JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID
  #include "renderer/pdfium/ayra_PdfiumRenderer.h"
#endif

//==============================================================================
// WIDGETS -- esclusi in modalita' headless
#ifndef AYRA_PDF_HEADLESS
  #include "widgets/ayra_PdfViewComponent.h"
  #include "widgets/look_and_feel/ayra_PdfLookAndFeelMethods.h"
  #include "widgets/look_and_feel/ayra_PdfDefaultLookAndFeel.h"

  // Legacy AppKit temporaneo fino al cutover current-only C01.
  #if JUCE_MAC
    #include "pdf_component/PDFComponent.h"
  #endif
#endif
