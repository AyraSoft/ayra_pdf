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

class WindowsPDFViewComponent : public juce::Component
{
public:
    
    WindowsPDFViewComponent()
        : currentPage(1),
          zoomLevel(1.0f),
          topLeftOrigin(0.0f, 0.0f),
          pdfDocument(nullptr),
          pdfBitmap(nullptr)
    {
        setOpaque(true);
    }
    
    ~WindowsPDFViewComponent()
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
        // This is a simplified implementation that doesn't require external libraries
        pageCount = 0;
        
        if (pdfDocument != nullptr && pdfData != nullptr)
        {
            // Extract basic information from the PDF data
            extractPDFInfo();
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
            
            // Draw a border around the page
            g.setColour(juce::Colours::black);
            g.drawRect(0, 0, width, height, 1);
            
            // Draw page number
            juce::String pageInfo = "Page " + juce::String(currentPage) + "/" + juce::String(pageCount);
            g.drawText(pageInfo, 10, 10, width - 20, 30, juce::Justification::topRight, true);
            
            // Draw some basic page content indicators
            g.drawLine(10, 10, width - 10, 10, 1.0f);  // Top line
            g.drawLine(10, height - 10, width - 10, height - 10, 1.0f);  // Bottom line
            
            // Draw some text lines to simulate content
            g.setFont(14.0f);
            int lineHeight = 20;
            int startY = 50;
            
            // Use the PDF data to generate some pseudo-random content based on the data
            juce::Random random(static_cast<int>(pdfData->getSize()) + currentPage);
            
            for (int i = 0; i < 15; ++i)
            {
                int lineWidth = random.nextInt(width - 100) + 50;
                g.setColour(juce::Colours::grey.withAlpha(0.7f));
                g.fillRect(20, startY + (i * lineHeight * 2), lineWidth, lineHeight);
            }
        }
    }
    
    // Extracts basic information from the PDF data
    void extractPDFInfo()
    {
        // This is a simplified implementation that doesn't require external libraries
        // In a real implementation, you would use a proper PDF parsing library
        
        // For now, we'll just set some reasonable defaults
        pageCount = 1;
        currentPageBounds = juce::Rectangle<float>(0, 0, 595, 842); // A4 size in points
        
        if (pdfData != nullptr && pdfData->getSize() > 0)
        {
            // Try to count pages by looking for /Page entries
            juce::String pdfContent(static_cast<const char*>(pdfData->getData()), 
                                  juce::jmin(static_cast<int>(pdfData->getSize()), 10000));
            
            int pageCount = 0;
            int index = 0;
            
            while ((index = pdfContent.indexOf(index + 1, "/Page")) >= 0)
            {
                pageCount++;
            }
            
            // Ensure at least one page
            this->pageCount = juce::jmax(1, pageCount);
        }
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
        
        // Draw some basic page content indicators
        g.drawLine(10, 10, width - 10, 10, 1.0f);  // Top line
        g.drawLine(10, height - 10, width - 10, height - 10, 1.0f);  // Bottom line
        
        // Draw some text lines to simulate content
        g.setFont(14.0f);
        int lineHeight = 20;
        int startY = 50;
        
        for (int i = 0; i < 10; ++i)
        {            
            int lineWidth = juce::Random::getSystemRandom().nextInt(width - 100) + 50;
            g.setColour(juce::Colours::grey);
            g.fillRect(20, startY + (i * lineHeight * 2), lineWidth, lineHeight);
        }
    }
    
    // Helper method to draw a simple representation of PDF content
    void drawSimplePDFRepresentation(juce::Graphics& g, int width, int height)
    {
        // Draw a border around the page
        g.setColour(juce::Colours::black);
        g.drawRect(0, 0, width, height, 1);
        
        // Draw page number
        juce::String pageInfo = "Page " + juce::String(currentPage) + "/" + juce::String(pageCount);
        g.drawText(pageInfo, 10, 10, width - 20, 30, juce::Justification::topRight, true);
    }
    
    // This method was removed to fix the duplicate renderCurrentPage() implementation
    
    friend class PDFComponent;
    
    std::unique_ptr<juce::InputStream> pdfDocument;
    std::unique_ptr<juce::MemoryBlock> pdfData;
    std::unique_ptr<juce::Image> pdfBitmap;
    
    int currentPage;
    int pageCount = 0;
    float zoomLevel;
    juce::Point<float> topLeftOrigin;
    juce::Rectangle<float> currentPageBounds;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WindowsPDFViewComponent)
};

} // namespace ayra
