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
 *  Il widget possiede un PdfDocument quando carica file/buffer direttamente, oppure puo'
 *  osservare un PdfDocument esterno non-owning. Il documento resta l'unica source of truth:
 *  pagina corrente, zoom, posizione e cache immagine sono solo stato di presentazione.
 *
 *  Tutti i metodi del widget devono essere chiamati dal JUCE Message Thread.
 *
 *  Per un documento esterno, il caller deve garantirne lifetime e immutabilita' per tutto il
 *  periodo in cui e' collegato. Per sostituirlo, chiamare nuovamente setDocument() oppure
 *  caricare un documento owned dal widget.
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
        noDocumentTextColourId = 0x3AB0004
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
    };

    class Listener
    {
    public:
        virtual ~Listener() = default;

        /** Evento di navigazione. newPage usa numerazione utente 1-based. */
        virtual void pdfPageChanged (PdfViewComponent* comp, int newPage) = 0;

        /** Evento emesso dopo l'attivazione riuscita di un nuovo documento. */
        virtual void pdfDocumentLoaded (PdfViewComponent* comp) {}
    };

    PdfViewComponent();
    ~PdfViewComponent() override;

    //==============================================================================
    // DOCUMENTO E NAVIGAZIONE

    void loadDocument (const juce::String& filePath);

    [[nodiscard]] bool thereIsADocumentLoaded() const;
    [[nodiscard]] int getTotPagesNum() const;

    /** Pagina visualizzata 1-based; 0 quando non esiste una pagina attiva. */
    [[nodiscard]] int getCurrentPageOnScreen() const;

    /** Naviga a una pagina utente 1-based. Valori fuori range non modificano lo stato. */
    void setPageNumber (int pageNumber);

    /** Dimensioni native visuali della pagina corrente, in punti PDF e dopo /Rotate. */
    [[nodiscard]] float getDocumentWidth() const;
    [[nodiscard]] float getDocumentHeight() const;

    //==============================================================================
    // VIEWPORT

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

    void exportCurrentDocument (const juce::String& withName,
                                const juce::String& folderPath) const;

    void loadDocumentFromMemoryBlock (const void* data, int sizeInBytes);

    /** Su failure destData resta invariato. */
    void getMemoryBlockFromDocument (juce::MemoryBlock& destData);

    /** Collega un documento esterno aperto con almeno una pagina.
     *  Il viewer non prende ownership: doc deve restare vivo e non mutare mentre e' collegato.
     */
    void setDocument (PdfDocument& doc);

    //==============================================================================
    // EVENTI

    void addListener (Listener* l);
    void removeListener (Listener* l);

    std::function<void (int)> onPageChanged;
    std::function<void()>     onDocumentLoaded;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    LookAndFeelMethods& getLAF();

    [[nodiscard]] PdfPage getCurrentPageInfo() const;

    void activateDocument (PdfDocument& doc);
    void invalidatePageCache();
    void notifyDocumentLoaded();
    void notifyPageChanged();

    std::unique_ptr<PdfDocument> ownedDocument;
    PdfDocument* currentDocument { nullptr };

    juce::Image cachedPageImage;
    bool pageCacheReady { false };

    float              currentZoom { 1.0f };
    juce::Point<float> topLeft {};
    int                currentPage { 0 };

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PdfViewComponent)
};

} // namespace ayra
