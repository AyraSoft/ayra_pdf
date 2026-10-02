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

#pragma once

namespace ayra
{

/** Viewer PDF cross-platform basato su PdfDocument.
 *
 *  Il documento attivo e' posseduto tramite std::shared_ptr. I load diretti creano il
 *  PdfDocument condiviso; setDocument() collega un documento gia' condiviso. Il documento
 *  resta l'unica source of truth: pagina, zoom, posizione e cache immagine sono view state.
 *
 *  Tutti i metodi pubblici del widget devono essere chiamati dal JUCE Message Thread.
 *  Il raster gira su un worker del modulo che cattura un owner del documento: il widget puo'
 *  cambiare documento o essere distrutto senza bloccare in attesa del render precedente.
 *
 *  Un PdfDocument condiviso non deve essere mutato concorrentemente dal caller mentre il
 *  viewer lo sta usando.
 *
 *  @see PdfDocument, PdfPage
 */
class PdfViewComponent : public juce::Component
{
public:
    enum ColourIds
    {
        backgroundColourId     = 0x3AB0001,
        pageColourId           = 0x3AB0002,
        shadowColourId         = 0x3AB0003,
        noDocumentTextColourId = 0x3AB0004,
        searchHighlightColourId = 0x3AB0005
    };

    /** Hook LookAndFeel del viewer.
     *  Le implementazioni base inoltrano al PdfDefaultLookAndFeel, quindi un LookAndFeel
     *  applicativo puo' override-are soltanto gli elementi che vuole personalizzare.
     */
    struct LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;

        virtual void drawPdfViewBackground (juce::Graphics&, int width, int height, PdfViewComponent&);
        virtual void drawPdfViewNoDocument (juce::Graphics&, int width, int height, PdfViewComponent&);
        virtual void drawPdfViewPageBackground (juce::Graphics&, juce::Rectangle<float> pageBounds, PdfViewComponent&);
        virtual void drawPdfViewPageShadow (juce::Graphics&, juce::Rectangle<float> pageBounds, PdfViewComponent&);
        virtual void drawPdfViewSearchHighlight (juce::Graphics&, juce::Rectangle<float> highlightBounds, PdfViewComponent&);
    };

    class Listener
    {
    public:
        virtual ~Listener() = default;

        /** Evento di navigazione. newPage usa numerazione utente 1-based. */
        virtual void pdfPageChanged (PdfViewComponent* comp, int newPage) = 0;

        /** Evento emesso dopo l'attivazione riuscita di un nuovo documento. */
        virtual void pdfDocumentLoaded (PdfViewComponent* comp) {}

        /** Evento emesso quando un documento attivo viene chiuso esplicitamente. */
        virtual void pdfDocumentClosed (PdfViewComponent* comp) {}

        /** Risultati highlight della pagina corrente aggiornati. */
        virtual void pdfSearchResultsChanged (PdfViewComponent* comp, int resultCount) {}
    };

    PdfViewComponent();
    ~PdfViewComponent() override;

    //==============================================================================
    // DOCUMENTO E NAVIGAZIONE

    void loadDocument (juce::String filePath);

    [[nodiscard]] bool thereIsADocumentLoaded() const;

    /** Numero pagine; -1 quando non esiste un documento attivo. */
    [[nodiscard]] int getTotPagesNum() const;

    /** Pagina visualizzata 1-based; -1 quando non esiste una pagina attiva. */
    [[nodiscard]] int getCurrentPageOnScreen() const;

    /** Naviga a una pagina utente 1-based. Valori fuori range non modificano lo stato. */
    void setPageNumber (int pageNumber);

    /** Dimensioni native visuali della pagina corrente, in punti PDF e dopo /Rotate. */
    [[nodiscard]] float getDocumentWidth() const;
    [[nodiscard]] float getDocumentHeight() const;

    //==============================================================================
    // VIEWPORT

    /** Zoom corrente; -1 quando non esiste un documento attivo. */
    [[nodiscard]] float getCurrentPageZoom() const;

    void setCurrentPageZoom (float zoom, juce::Point<float> handlePoint);

    inline void setCurrentPageZoom (float zoom, juce::Point<int> handlePoint)
    {
        setCurrentPageZoom (zoom, handlePoint.toFloat());
    }

    [[nodiscard]] juce::Point<float> getCurrentPageTopLeftPosition() const;
    void setCurrentPageTopLeftPosition (juce::Point<float> newPos);

    inline void setCurrentPageTopLeftPosition (juce::Point<int> newPos)
    {
        setCurrentPageTopLeftPosition (newPos.toFloat());
    }

    /** Bounds logici della pagina nel widget, con rotazione e zoom applicati. */
    [[nodiscard]] juce::Rectangle<float> getCurrentPageBounds() const;

    //==============================================================================
    // I/O

    void exportCurrentDocument (juce::String withName,
                                juce::String folderPath) const;

    void loadDocumentFromMemoryBlock (const void* data, int sizeInBytes);

    /** Su failure destData resta invariato. */
    void getMemoryBlockFromDocument (juce::MemoryBlock& destData);

    /** Collega un documento condiviso aperto con almeno una pagina.
     *  Il viewer trattiene ownership condivisa anche per i render asincroni gia' accodati.
     *  Passare nullptr chiude il documento corrente e invalida render/search pendenti.
     */
    void setDocument (std::shared_ptr<PdfDocument> doc);

    //==============================================================================
    // SEARCH OVERLAY

    /** Imposta la query evidenziata sulla pagina corrente. Stringa vuota = clear. */
    void setSearchQuery (const juce::String& query);

    void clearSearch();

    [[nodiscard]] int getSearchResultCount() const noexcept;

    //==============================================================================
    // EVENTI

    void addListener (Listener* l);
    void removeListener (Listener* l);

    std::function<void (int)> onPageChanged;
    std::function<void()>      onDocumentLoaded;
    std::function<void()>      onDocumentClosed;
    std::function<void (int)>  onSearchResultsChanged;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;
    void moved() override;
    void parentHierarchyChanged() override;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override;
    void mouseMagnify (const juce::MouseEvent& event, float scaleFactor) override;

private:
    LookAndFeelMethods& getLAF();

    [[nodiscard]] PdfPage getCurrentPageInfo() const;

    struct RenderState;

    void activateDocument (std::shared_ptr<PdfDocument> doc);
    void invalidatePageCache();
    void requestPageRender();
    void publishPageRender (std::uint64_t generation, PdfPage page, juce::Image image);

    void invalidateSearchResults();
    void requestSearchResults();
    void publishSearchResults (std::uint64_t generation,
                               int pageIndex,
                               juce::Array<PdfSearchResult> results);

    void updateRasterDeviceScale();
    void clampTopLeft();
    [[nodiscard]] juce::Rectangle<float> pdfBoundsToWidget (const PdfPage& page,
                                                            juce::Rectangle<float> pdfBounds) const;

    void notifyDocumentLoaded();
    void notifyDocumentClosed();
    void notifyPageChanged();
    void notifySearchResultsChanged();

    std::shared_ptr<PdfDocument> currentDocument;
    std::shared_ptr<RenderState> renderState;

    juce::Image cachedPageImage;
    PdfPage currentPageInfo;
    int currentPageCount { 0 };

    juce::String searchQuery;
    juce::Array<PdfSearchResult> searchResults;

    float currentZoom { 1.0f };
    float rasterDeviceScale { 1.0f };
    juce::Point<float> topLeft {};
    juce::Point<float> lastDragPosition {};
    int currentPage { 0 };

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PdfViewComponent)
};

} // namespace ayra
