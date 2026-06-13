/*
     ______  __    __  ______   ______
    |      \|  \  |  \/      \ |      \
     \▓▓▓▓▓▓\ ▓▓  | ▓▓  ▓▓▓▓▓▓\ \▓▓▓▓▓▓\
     /      ▓▓ ▓▓  | ▓▓ ▓▓   \▓▓/      ▓▓   Copyright 2023
    |  ▓▓▓▓▓▓▓ ▓▓__/ ▓▓ ▓▓     |  ▓▓▓▓▓▓▓   Ayra Soft
     \▓▓    ▓▓\▓▓    ▓▓ ▓▓      \▓▓    ▓▓   www.ayra.live
      \▓▓▓▓▓▓▓_\▓▓▓▓▓▓▓\▓▓       \▓▓▓▓▓▓▓
             |  \__| ▓▓
              \▓▓    ▓▓
                \▓▓▓▓▓▓
    
 Ayra uses a GPL/commercial licence - see LICENCE.md for details.
*/

/** @deprecated Usa ayra::PdfViewComponent invece di PDFComponent.
 *  Questo file rimane per backward compatibility.
 *  Verra' rimosso nella Fase 5 del refactoring di ayra_pdf.
 *  @see ayra_PdfViewComponent.h
 */

namespace ayra
{

class MacPDFViewComponent;
class WindowsPDFViewComponent;
class LinuxPDFViewComponent;

class PDFComponent : public juce::Component
{
public:
    PDFComponent();
    ~PDFComponent();
    
    //==============================================================================
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    //==============================================================================

    void loadDocument(const juce::String filePath);
    
    //==============================================================================
    
    bool thereIsADocumentLoaded() const;
    
    //==============================================================================

    int getTotPagesNum() const;
    int getCurrentPageOnScreen() const;
    void setPageNumber(int pageNumber);
    
    //==============================================================================
    
    float getDocumentWidth() const; // restituisce la vera larghezza del documento
    float getDocumentHeight() const; // restituisce la vera altezza del documento
//    float getCurrentPageSizeRatio() const;
    
    float getCurrentPageZoom() const;
    void setCurrentPageZoom(const float zoom, const juce::Point<float> handlePoint);
    inline void setCurrentPageZoom(const float zoom, const juce::Point<int> handlePoint) { setCurrentPageZoom(zoom, handlePoint.toFloat()); }
    
    juce::Point<float> getCurrentPageTopLeftPosition() const;
    void setCurrentPageTopLeftPosition(const juce::Point<float> newPos);
    inline void setCurrentPageTopLeftPosition(const juce::Point<int> newPos) { setCurrentPageTopLeftPosition(newPos.toFloat()); }

    juce::Rectangle<float> getCurrentPageBounds() const;
    
    //==============================================================================
    
    void exportCurrentDocument(const juce::String withName, const juce::String containingFolderPath) const;
    
    void loadDocumentFromMemoryBlock(const void* data, int sizeInBytes);
    void getMemoryBlockFromDocument(juce::MemoryBlock& destData);
    
private:
    
#if JUCE_IOS || JUCE_MAC
    std::unique_ptr<MacPDFViewComponent>     pdfViewComponent;
#elif JUCE_WINDOWS
    std::unique_ptr<WindowsPDFViewComponent> pdfViewComponent;
#elif JUCE_ANDROID || JUCE_LINUX
    std::unique_ptr<LinuxPDFViewComponent>   pdfViewComponent;
#endif
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PDFComponent)
};

} // namespace ayra
