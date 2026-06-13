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
                        macOS/iOS usa CoreGraphics nativo. Windows/Linux/Android usa PDFium (Apache 2.0).
  website:
  license:              Copyright. All Rights Reserved.

  dependencies:         juce_gui_extra

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#define JUCE_AYRA_PDF_H_INCLUDED

#include <juce_gui_extra/juce_gui_extra.h>

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
#endif

//==============================================================================
// BACKWARD COMPATIBILITY -- Fase 5: decommentare quando PdfViewComponent e' completo
// namespace ayra { using PDFComponent = PdfViewComponent; }
// Per ora PDFComponent e' ancora in pdf_component/PDFComponent.h (LEGACY)
#include "pdf_component/PDFComponent.h"
