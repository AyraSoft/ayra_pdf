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

//==============================================================================
// RENDERER PLATFORM-SPECIFICO

#if JUCE_MAC || JUCE_IOS
  // Renderer nativo CoreGraphics (Fase 2: migrazione completa)
  // Per ora include il vecchio MacPDFComponent per backward compat
  #include <CoreGraphics/CoreGraphics.h>
  #include <Cocoa/Cocoa.h>
  #include "pdf_component/Mac_PDF_core/PDFView.m"
  #include "pdf_component/Mac_PDF_core/MacPDFComponent.mm"
  #include "renderer/mac/ayra_MacPdfRenderer.mm"  // skeleton Fase 2

#elif JUCE_WINDOWS
  // Renderer PDFium (Fase 3: implementazione completa)
  #include "renderer/pdfium/ayra_PdfiumRenderer.cpp"  // skeleton Fase 3
  // LEGACY -- rimosso in Fase 5 quando PdfViewComponent sara' completo
  // #include "pdf_component/Windows_PDF_core/WindowsPDFViewComponent.h"

#elif JUCE_ANDROID || JUCE_LINUX
  // Renderer PDFium (Fase 3: implementazione completa)
  #include "renderer/pdfium/ayra_PdfiumRenderer.cpp"  // skeleton Fase 3
  // LEGACY -- rimosso in Fase 5 quando PdfViewComponent sara' completo
  // #include "pdf_component/Linux_PDF_core/LinuxPDFViewComponent.h"
#endif

//==============================================================================
// ENGINE
#include "engine/ayra_PdfDocument.cpp"

//==============================================================================
// WIDGETS
#ifndef AYRA_PDF_HEADLESS
  #include "widgets/ayra_PdfViewComponent.cpp"
  #include "pdf_component/PDFComponent.cpp"  // LEGACY backward compat
#endif
