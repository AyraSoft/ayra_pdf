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

#ifdef JUCE_AYRA_PDF_H_INCLUDED
 #error "Incorrect use of JUCE cpp file"
#endif

#include "ayra_pdf.h"

#include "renderer/ayra_PdfSafetyLimits.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <vector>

//==============================================================================
// RENDERER PLATFORM-SPECIFICO

#if JUCE_MAC || JUCE_IOS
  #include <CoreGraphics/CoreGraphics.h>
  #import <PDFKit/PDFKit.h>

  #if JUCE_MAC
    #include <Cocoa/Cocoa.h>
    #include "pdf_component/Mac_PDF_core/PDFView.m"
    #include "pdf_component/Mac_PDF_core/MacPDFComponent.mm"
  #endif

  #include "renderer/mac/ayra_MacPdfRenderer.mm"

#elif JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID
  #include "third_party/pdfium/include/fpdfview.h"
  #include "third_party/pdfium/include/fpdf_edit.h"
  #include "third_party/pdfium/include/fpdf_transformpage.h"
  #include "third_party/pdfium/include/fpdf_text.h"
  #include "renderer/pdfium/ayra_PdfiumRenderer.cpp"
#endif

//==============================================================================
// ENGINE
#include "engine/ayra_PdfDocument.cpp"

//==============================================================================
// WIDGETS
#ifndef AYRA_PDF_HEADLESS
  #include "widgets/ayra_PdfViewComponent.cpp"

  #if JUCE_MAC
    #include "pdf_component/PDFComponent.cpp"
  #endif
#endif
