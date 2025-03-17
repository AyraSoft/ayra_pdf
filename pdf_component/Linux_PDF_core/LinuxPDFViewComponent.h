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


//MARK: PDFViewComponent ==============================================================================

class LinuxPDFViewComponent : public juce::Component
{
public:
    
    LinuxPDFViewComponent()
        : currentPage(1),
          zoomLevel(1.0f),
          topLeftOrigin(0.0f, 0.0f),
          pdfDocument(nullptr),
          pdfBitmap(nullptr)
    {
        setOpaque(true);
    }
    
    ~LinuxPDFViewComponent()
    {
        closeDocument();
    }
    
    bool thereIsADocumentLoaded() const
    {
        return getTotPagesNum() >= 1;
    }
    
    void loadDocument(const juce::String filePath)
    {
        closeDocument();
        
        // Use JUCE to load the PDF document
        pdfDocument = std::make_unique<juce::FileInputStream>(filePath);
        
        if (pdfDocument != nullptr && pdfDocument->openedOk())
        {            
            // Read the PDF header to verify it's a valid PDF
            char header[5] = {0};
            pdfDocument->read(header, 4);
            
            if (juce::String(header) != "%PDF")
            {
                closeDocument();
                return;
            }
            
            // Reset file position
            pdfDocument->setPosition(0);
            
            // Load the entire PDF into memory
            pdfData.reset(new juce::MemoryBlock(pdfDocument->getTotalLength()));
            pdfDocument->readIntoMemoryBlock(*pdfData);
            
            // Parse the PDF to count pages
            parsePdfStructure();
            
            setPageNumber(1);
        }
    }
    
    void setPageNumber(const int pageNumber)
    {
        if (pdfDocument != nullptr)
        {
            if (pageNumber >= 1 && pageNumber <= getTotPagesNum())
            {
                currentPage = pageNumber;
                zoomLevel = 1.0f;
                topLeftOrigin = juce::Point<float>(0.0f, 0.0f);
                
                renderCurrentPage();
                repaint();
            }
        }
    }
    
    int getTotPagesNum() const
    {
        if (pdfDocument != nullptr && pdfData != nullptr)
        {
            return pageCount;
        }
        
        return -1;
    }
    
    int getCurrentPageOnScreen() const
    {
        if (pdfDocument == nullptr) { return -1; }
        return currentPage;
    }
    
    void setPageNumber(const int pageNumber)
    {
//        if (pdfView.pdfDocument != NULL)
//        {
//            if (pageNumber >= 1 && pageNumber <= getTotPagesNum())
//            {
//                pdfView.currentPage = pageNumber;
//                
//                pdfView.zoomLevel = 1.0f;
//                pdfView.topLeftOrigin = CGPointMake(0.0, 0.0);
//                
//                [pdfView setNeedsDisplay:YES];
//            }
//        }
    }
    
    int getTotPagesNum() const
    {
//        if (pdfView.pdfDocument != NULL)
//        {
//            return (int)CGPDFDocumentGetNumberOfPages(pdfView.pdfDocument);
//        }
//        
        return -1;
    }
    
    int getCurrentPageOnScreen() const
    {
//        if (pdfView.pdfDocument == NULL) { return -1; }
//        return (int)pdfView.currentPage;
        return -1;
    }
    
    float getDocumentWidth() const
    {
        if (pdfDocument != nullptr && currentPageBounds.getWidth() > 0)
        {
            return currentPageBounds.getWidth();
        }
        
        return 0.0f;
    }

    float getDocumentHeight() const
    {
        if (pdfDocument != nullptr && currentPageBounds.getHeight() > 0)
        {
            return currentPageBounds.getHeight();
        }
        
        return 0.0f;
    }
    
    float getCurrentPageZoom() const
    {
        if (pdfDocument != nullptr)
        {
            return zoomLevel;
        }
        
        return -1;
    }
    
    void setCurrentPageZoom(const float zoom, const juce::Point<float> handlePoint)
    {
        if (pdfDocument != nullptr)
        {
            float oldZoom = zoomLevel;
            zoomLevel = zoom;
            
            juce::Point<float> origin = getCurrentPageTopLeftPosition();
            
            float xOffset = handlePoint.getX() - (handlePoint.getX() * zoom / oldZoom);
            float yOffset = handlePoint.getY() - (handlePoint.getY() * zoom / oldZoom);
            
            topLeftOrigin = juce::Point<float>(origin.getX() + xOffset, origin.getY() + yOffset);
            
            renderCurrentPage();
            repaint();
        }
    }
    
    juce::Rectangle<float> getCurrentPageBounds() const
    {
        if (pdfDocument != nullptr)
        {
            return currentPageBounds;
        }
        
        return juce::Rectangle<float> {};
    }
    
    juce::Point<float> getCurrentPageTopLeftPosition() const
    {
        if (pdfDocument != nullptr)
        {
            return topLeftOrigin;
        }
        
        return juce::Point<float> {};
    }
    
    void setCurrentPageTopLeftPosition(juce::Point<float> newPos)
    {
        if (pdfDocument != nullptr)
        {
            topLeftOrigin = newPos;
            repaint();
        }
    }
    
    void exportCurrentDocument(const juce::String withName, const juce::String containingFolderPath)
    {
        if (pdfDocument != nullptr && pdfData != nullptr)
        {
            juce::File outputFile(containingFolderPath + "/" + withName + ".pdf");
            juce::FileOutputStream outputStream(outputFile);
            
            if (outputStream.openedOk())
            {
                // Simply write the PDF data to the file
                outputStream.write(pdfData->getData(), pdfData->getSize());
                outputStream.flush();
            }
        }
    }
    
    void loadDocumentFromMemoryBlock(const void* data, int sizeInBytes)
    {
        closeDocument();
        
        // Create a new memory block with the provided data
        pdfData = std::make_unique<juce::MemoryBlock>(data, sizeInBytes);
        
        // Create a memory input stream to read the PDF data
        pdfDocument = std::make_unique<juce::MemoryInputStream>(*pdfData, false);
        
        // Verify it's a valid PDF
        char header[5] = {0};
        pdfDocument->read(header, 4);
        
        if (juce::String(header) != "%PDF")
        {
            closeDocument();
            return;
        }
        
        // Reset position
        pdfDocument->setPosition(0);
        
        // Parse the PDF structure
        parsePdfStructure();
        
        setPageNumber(1);
    }
    
    void getMemoryBlockFromDocument(juce::MemoryBlock& destData)
    {
        if (pdfData != nullptr)
        {
            // Simply copy the PDF data to the destination
            destData.replaceAll(pdfData->getData(), pdfData->getSize());
        }
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::white);
        
        if (pdfBitmap != nullptr)
        {
            // Calculate the position to draw the PDF
            float x = topLeftOrigin.getX();
            float y = topLeftOrigin.getY();
            float width = pdfBitmap->getWidth() * zoomLevel;
            float height = pdfBitmap->getHeight() * zoomLevel;
            
            g.drawImageTransformed(*pdfBitmap, 
                                   juce::AffineTransform::scale(zoomLevel).translated(x, y));
        }
    }
    
private:
    void closeDocument()
    {
        pdfDocument.reset();
        pdfData.reset();
        pdfBitmap.reset();
        pageCount = 0;
        currentPage = 1;
        zoomLevel = 1.0f;
        topLeftOrigin = juce::Point<float>(0.0f, 0.0f);
    }
    
    void parsePdfStructure()
    {
        // Basic PDF structure parsing to count pages
        // This is a simplified implementation
        pageCount = 0;
        
        if (pdfDocument != nullptr && pdfData != nullptr)
        {
            juce::String pdfContent(static_cast<const char*>(pdfData->getData()), pdfData->getSize());
            
            // Count occurrences of "/Page" to estimate page count
            int index = 0;
            while ((index = pdfContent.indexOf(index + 1, "/Page")) >= 0)
            {
                pageCount++;
            }
            
            // Ensure at least one page
            pageCount = juce::jmax(1, pageCount);
            
            // Set default page bounds based on A4 size
            currentPageBounds = juce::Rectangle<float>(0, 0, 595, 842); // A4 in points
            
            // Try to extract actual page size from PDF
            extractPageBounds();
        }
    }
    
    void extractPageBounds()
    {
        // Simplified implementation to extract page bounds
        // In a real implementation, this would parse the PDF structure more thoroughly
        if (pdfDocument != nullptr && pdfData != nullptr)
        {
            // Default to A4 if we can't determine size
            currentPageBounds = juce::Rectangle<float>(0, 0, 595, 842);
        }
    }
    
    void renderCurrentPage()
    {
        if (pdfDocument != nullptr && pdfData != nullptr)
        {
            // Create a bitmap to render the PDF page
            int width = static_cast<int>(currentPageBounds.getWidth());
            int height = static_cast<int>(currentPageBounds.getHeight());
            
            pdfBitmap = std::make_unique<juce::Image>(juce::Image::RGB, width, height, true);
            juce::Graphics g(*pdfBitmap);
            
            // Fill with white background
            g.fillAll(juce::Colours::white);
            
            #if JUCE_LINUX
                renderPDFWithLinuxAPI(g, width, height);
            #else
                // Fallback for non-Linux platforms
                renderFallbackPage(g, width, height);
            #endif
        }
    }
    
    // Creates a temporary file with the PDF data for rendering
    juce::File createTemporaryPDFFile()
    {
        juce::TemporaryFile tempFile;
        juce::File pdfFile = tempFile.getFile();
        
        // Write PDF data to the temporary file
        juce::FileOutputStream outputStream(pdfFile);
        if (outputStream.openedOk())
        {
            outputStream.write(pdfData->getData(), pdfData->getSize());
            outputStream.flush();
        }
        
        return pdfFile;
    }
    
    // Renders a fallback page when PDF rendering fails
    void renderFallbackPage(juce::Graphics& g, int width, int height, const juce::String& errorMessage = {})
    {
        g.setColour(juce::Colours::black);
        g.drawRect(0, 0, width, height, 1);
        
        juce::String message = "PDF Page " + juce::String(currentPage) + "/" + juce::String(pageCount);
        if (errorMessage.isNotEmpty())
            message += " (" + errorMessage + ")";
            
        g.drawText(message, 0, 0, width, height, juce::Justification::centred);
    }
    
    // Renders the PDF using Linux-specific APIs (Poppler via pdftocairo)
    void renderPDFWithLinuxAPI(juce::Graphics& g, int width, int height)
    {
        // Create a temporary file to write the PDF data
        juce::File pdfFile = createTemporaryPDFFile();
        
        // Create a temporary file for the rendered image
        juce::TemporaryFile tempImageFile;
        juce::File imageFile = tempImageFile.getFile().withFileExtension("png");
        
        // Render PDF to image using pdftocairo (from Poppler)
        if (renderPDFUsingPdftocairo(pdfFile, imageFile, width))
        {
            // Load and display the rendered image
            displayRenderedImage(g, imageFile, width, height);
        }
        else
        {
            // Fallback if rendering failed
            renderFallbackPage(g, width, height, "Rendering Failed");
        }
    }
    
    // Uses pdftocairo to render a PDF to an image file
    bool renderPDFUsingPdftocairo(const juce::File& pdfFile, const juce::File& imageFile, int width)
    {
        juce::String command = "pdftocairo -png -singlefile -scale-to " + 
                              juce::String(width) + " " + 
                              pdfFile.getFullPathName() + " " + 
                              imageFile.getFullPathName().upToLastOccurrenceOf(".", false, false);
        
        int result = system(command.toRawUTF8());
        
        return (result == 0 && imageFile.existsAsFile());
    }
    
    // Loads and displays a rendered image
    void displayRenderedImage(juce::Graphics& g, const juce::File& imageFile, int width, int height)
    {
        juce::Image renderedImage = juce::ImageFileFormat::loadFrom(imageFile);
        
        if (renderedImage.isValid())
        {
            // Draw the rendered image to our bitmap
            g.drawImageAt(renderedImage, 0, 0);
        }
        else
        {
            // Fallback if image loading failed
            renderFallbackPage(g, width, height, "Image Loading Failed");
        }
    }
    
    friend class PDFComponent;
    
    std::unique_ptr<juce::InputStream> pdfDocument;
    std::unique_ptr<juce::MemoryBlock> pdfData;
    std::unique_ptr<juce::Image> pdfBitmap;
    
    int currentPage;
    int pageCount = 0;
    float zoomLevel;
    juce::Point<float> topLeftOrigin;
    juce::Rectangle<float> currentPageBounds;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LinuxPDFViewComponent)
};

} // namespace ayra
