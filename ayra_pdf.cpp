
#ifdef JUCE_AYRA_PDF_H_INCLUDED
 #error "Incorrect use of JUCE cpp file"
#endif

#include "ayra_pdf.h"

#if JUCE_IOS || JUCE_MAC
#include <CoreGraphics/CoreGraphics.h>
#include <Cocoa/Cocoa.h>
#include "pdf_component/Mac_PDF_core/PDFView.m"
#include "pdf_component/Mac_PDF_core/MacPDFComponent.mm"
#elif JUCE_WINDOWS
#include "pdf_component/Windows_PDF_core/WindowsPDFViewComponent.h"
#elif JUCE_ANDROID || JUCE_LINUX
#include "pdf_component/Linux_PDF_core/LinuxPDFViewComponent.h"
#endif

#include "pdf_component/PDFComponent.cpp"


