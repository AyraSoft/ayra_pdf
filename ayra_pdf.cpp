
#ifdef JUCE_AYRA_PDF_H_INCLUDED
 #error "Incorrect use of JUCE cpp file"
#endif

#include "ayra_pdf.h"

#if JUCE_IOS || JUCE_MAC
#include <CoreGraphics/CoreGraphics.h>b
#include <Cocoa/Cocoa.h>
#include "pdf_component/Mac_PDF_core/PDFView.m"
#include "pdf_component/Mac_PDF_core/MacPDFComponent.mm"
#elif JUCE_WINDOWS
#elif JUCE_ANDROID || JUCE_LINUX
#endif

#include "pdf_component/PdfComponent.cpp"

