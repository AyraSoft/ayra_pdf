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


namespace ayra
{

//MARK: Static Funcs ==============================================================================

static inline NSString* juceStringToNS (const juce::String& str)
{
    return [NSString stringWithUTF8String: str.toUTF8()];
}

static inline juce::String NSToJuceString (const NSString* str)
{
    return juce::String { str.UTF8String };
}

//MARK: PDFViewComponent ==============================================================================

class MacPDFViewComponent :
                            #if JUCE_IOS
                            public juce::UIViewComponent
                            #elif JUCE_MAC
                            public juce::NSViewComponent
                            #endif
{
public:
    
    MacPDFViewComponent()
    {
        pdfView = [[PDFView alloc] initWithFrame:NSMakeRect(0, 0, getWidth(), getHeight())];
        pdfView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
//        previousArea = getLocalBounds();
//        setCurrentPageBounds(previousArea.toFloat());
        setView(pdfView);
    }
    
    ~MacPDFViewComponent()
    {
        [pdfView release];
    }
    
    bool thereIsADocumentLoaded() const
    {
        return getTotPagesNum() >= 1;
    }
    
    void loadDocument(const juce::String filePath)
    {
        if (pdfView.pdfDocument != NULL) { CGPDFDocumentRelease(pdfView.pdfDocument); }
        
        NSString* path = juceStringToNS(filePath);
        NSURL *pdfURL = [NSURL fileURLWithPath:path];
        CGPDFDocumentRef pdfDocument = CGPDFDocumentCreateWithURL((__bridge CFURLRef)pdfURL);
        pdfView.pdfDocument = pdfDocument;
        
        [pdfView checkCurrentpage];
        
        setPageNumber(1);
    }
    
    void setPageNumber(const int pageNumber)
    {
        if (pdfView.pdfDocument != NULL)
        {
            if (pageNumber >= 1 && pageNumber <= getTotPagesNum())
            {
                pdfView.currentPage = pageNumber;
                
                pdfView.zoomLevel = 1.0f;
                pdfView.topLeftOrigin = CGPointMake(0.0, 0.0);
                
                [pdfView setNeedsDisplay:YES];
            }
        }
    }
    
    int getTotPagesNum() const
    {
        if (pdfView.pdfDocument != NULL)
        {
            return (int)CGPDFDocumentGetNumberOfPages(pdfView.pdfDocument);
        }
        
        return -1;
    }
    
    int getCurrentPageOnScreen() const
    {
        if (pdfView.pdfDocument == NULL) { return -1; }
        return (int)pdfView.currentPage;
    }
    
    float getDocumentWidth() const
    {
        if (pdfView.pdfDocument != NULL)
        {
            int currentPageNumber = getCurrentPageOnScreen();
            CGPDFPageRef pdfPage = CGPDFDocumentGetPage(pdfView.pdfDocument, currentPageNumber);
            
            if (pdfPage != NULL)
            {
                CGRect pageBounds = [pdfView getCurrentPageBounds];
                return (float)pageBounds.size.width;
            }
        }
        
        return 0.0f;
    }

    float getDocumentHeight() const
    {
        if (pdfView.pdfDocument != NULL)
        {
            int currentPageNumber = getCurrentPageOnScreen();
            CGPDFPageRef pdfPage = CGPDFDocumentGetPage(pdfView.pdfDocument, currentPageNumber);
            
            if (pdfPage != NULL)
            {
                CGRect pageBounds = [pdfView getCurrentPageBounds];
                return (float)pageBounds.size.height;
            }
        }
        
        return 0.0f;
    }
    
    float getCurrentPageZoom() const
    {
        if (pdfView.pdfDocument != NULL)
        {
            return pdfView.zoomLevel;
        }
        
        return -1;
    }
    
    void setCurrentPageZoom(const float zoom, const juce::Point<float> handlePoint)
    {
        if (pdfView.pdfDocument != NULL)
        {
            pdfView.zoomLevel = zoom;
            
            juce::Point<float> origin = getCurrentPageTopLeftPosition();
            
            float xOffset = handlePoint.getX() - (handlePoint.getX() * pdfView.zoomLevel);
            float yOffset = handlePoint.getY() - (handlePoint.getY() * pdfView.zoomLevel);
            
            pdfView.topLeftOrigin = CGPointMake(origin.getX() + xOffset, origin.getY() + yOffset);
            [pdfView setNeedsDisplay:YES];
        }
    }
    
    juce::Rectangle<float> getCurrentPageBounds() const
    {
        if (pdfView.pdfDocument != NULL)
        {
            int currentPageNumber = getCurrentPageOnScreen();
            CGPDFPageRef pdfPage = CGPDFDocumentGetPage(pdfView.pdfDocument, currentPageNumber);
            
            if (pdfPage != NULL)
            {
                CGRect mediaBox = CGPDFPageGetBoxRect(pdfPage, kCGPDFMediaBox);
                return juce::Rectangle<float> { (float)mediaBox.origin.x,
                    (float)mediaBox.origin.y,
                    (float)mediaBox.size.width,
                    (float)mediaBox.size.height };
            }
        }
        
        return juce::Rectangle<float> {};
    }
    
    juce::Point<float> getCurrentPageTopLeftPosition() const
    {
        if (pdfView.pdfDocument != NULL)
        {
            return juce::Point<float> { (float)pdfView.topLeftOrigin.x, (float)-pdfView.topLeftOrigin.y };
        }
        
        return juce::Point<float> {};
    }
    
    void setCurrentPageTopLeftPosition(juce::Point<float> newPos)
    {
        if (pdfView.pdfDocument != NULL)
        {
            pdfView.topLeftOrigin = CGPointMake(newPos.getX(), -newPos.getY());
            [pdfView setNeedsDisplay:YES];
        }
    }
    
    void exportCurrentDocument(const juce::String withName, const juce::String containingFolderPath)
    {
        if (pdfView.pdfDocument != NULL)
        {
            NSString* outputPath = juceStringToNS(containingFolderPath + "/" + withName + ".pdf");
            
            // Creare un contesto di grafica PDF per esportare il documento
            CGContextRef pdfContext = CGPDFContextCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:outputPath], NULL, NULL);

            if (pdfContext)
            {
                size_t numPages = CGPDFDocumentGetNumberOfPages(pdfView.pdfDocument);
                for (size_t pageNumber = 1; pageNumber <= numPages; ++pageNumber)
                {
                    CGPDFPageRef pdfPage = CGPDFDocumentGetPage(pdfView.pdfDocument, pageNumber);
                    CGRect mediaBox = CGPDFPageGetBoxRect(pdfPage, kCGPDFMediaBox);
                    
                    CGContextBeginPage(pdfContext, &mediaBox);
                    CGContextDrawPDFPage(pdfContext, pdfPage);
                    CGContextEndPage(pdfContext);
                }

                CGPDFContextClose(pdfContext);

                CGContextRelease(pdfContext);
            }
        }
    }
    
    void loadDocumentFromMemoryBlock(const void* data, int sizeInBytes)
    {
        NSData* pdfData = [NSData dataWithBytes:data length:sizeInBytes];
        CGDataProviderRef dataProvider = CGDataProviderCreateWithCFData((__bridge CFDataRef)pdfData);
        CGPDFDocumentRef newPdfDocument = CGPDFDocumentCreateWithProvider(dataProvider);
        
        if (pdfView.pdfDocument != NULL &&
            pdfView.pdfDocument != newPdfDocument)
        {
            CGPDFDocumentRelease(pdfView.pdfDocument);
        }

        pdfView.pdfDocument = newPdfDocument;
        
        CGDataProviderRelease(dataProvider);
        
        [pdfView setNeedsDisplay:YES];
    }
    
    void getMemoryBlockFromDocument(juce::MemoryBlock& destData)
    {
        if (pdfView.pdfDocument != NULL)
        {
            NSMutableData *pdfData = [NSMutableData data];
            CGContextRef pdfContext = CGPDFContextCreateWithURL((__bridge CFURLRef)[NSURL URLWithString:@"dummyURL"], NULL, NULL);
            CGPDFContextBeginPage(pdfContext, NULL);

            size_t numPages = CGPDFDocumentGetNumberOfPages(pdfView.pdfDocument);
            for (size_t pageNumber = 1; pageNumber <= numPages; ++pageNumber)
            {
                CGPDFPageRef pdfPage = CGPDFDocumentGetPage(pdfView.pdfDocument, pageNumber);
                CGRect mediaBox = CGPDFPageGetBoxRect(pdfPage, kCGPDFMediaBox);

                CGContextBeginPage(pdfContext, &mediaBox);
                CGContextDrawPDFPage(pdfContext, pdfPage);
                CGContextEndPage(pdfContext);

                [pdfData appendData:(NSData *)CGDataConsumerCreateWithCFData((__bridge CFMutableDataRef)pdfData)];
            }

            CGPDFContextEndPage(pdfContext);
            CGPDFContextClose(pdfContext);

            CGContextRelease(pdfContext);

            destData = juce::MemoryBlock { (void*)pdfData, sizeof(CGPDFDocumentRef) };
        }
    }
    
//    void resized() override
//    {
//    #if JUCE_IOS
//        juce::UIViewComponent::resized();
//    #elif JUCE_MAC
//        juce::NSViewComponent::resized();
//    #endif
//        
//        if (pdfView.pdfDocument)
//        {
//            juce::Rectangle<int> area = getLocalBounds();
//            DBG("ryryry");
//            DBG(area.getX());
//            DBG(area.getY());
//            pdfView.currentBounds = CGRectMake(area.getX(), area.getY(), area.getWidth(), area.getHeight());
//        }
//    }
    
private:
    
    friend class PDFComponent;
    
    PDFView* pdfView;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacPDFViewComponent)
};

} // namespace ayra
