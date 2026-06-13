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

/** Widget per la visualizzazione di documenti PDF.
 *
 *  Visualizzatore PDF cross-platform con supporto a zoom, pan e navigazione
 *  tra le pagine. Retrocompatibile con l'API di PDFComponent: i metodi
 *  pubblici originali sono mantenuti con firma identica.
 *
 *  Internamente usa PdfDocument, che sceglie automaticamente il renderer
 *  corretto per la piattaforma (MacPdfRenderer su macOS/iOS, PdfiumRenderer
 *  su Windows/Linux/Android).
 *
 *  ### Modalita' d'uso
 *
 *  **Documento owned** (caso comune — il widget carica e possiede il file):
 *  @code
 *  ayra::PdfViewComponent viewer;
 *  viewer.loadDocument ("/path/to/doc.pdf");
 *  addAndMakeVisible (viewer);
 *  @endcode
 *
 *  **Documento esterno** (il documento e' gestito esternamente):
 *  @code
 *  ayra::PdfDocument sharedDoc;
 *  sharedDoc.open (file);
 *  ayra::PdfViewComponent viewer;
 *  viewer.setDocument (sharedDoc);  // viewer non possiede il documento
 *  @endcode
 *
 *  ### Customizzazione visiva
 *  Implementa PdfViewComponent::LookAndFeelMethods nel tuo LookAndFeel per
 *  personalizzare sfondi, ombre e testo "nessun documento".
 *
 *  Fase 5: implementazione completa (per ora delega al vecchio PDFComponent).
 *
 *  @see PdfDocument, PDFComponent (LEGACY)
 */
class PdfViewComponent : public juce::Component
{
public:
    //==============================================================================
    /// Colour IDs nel namespace 0x3AB0xxx (convenzione ayra_pdf).
    enum ColourIds
    {
        backgroundColourId    = 0x3AB0001,  ///< Colore dello sfondo attorno alla pagina
        pageColourId          = 0x3AB0002,  ///< Colore di sfondo della pagina (quando il PDF non ha sfondo)
        shadowColourId        = 0x3AB0003,  ///< Colore dell'ombra sotto la pagina
        noDocumentTextColourId = 0x3AB0004  ///< Colore del testo "Nessun documento caricato"
    };

    //==============================================================================
    /// Interfaccia LookAndFeel per la customizzazione del rendering visivo.
    struct LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;

        /** Disegna lo sfondo del widget (area attorno alla pagina PDF). */
        virtual void drawPdfViewBackground (juce::Graphics& g, int width, int height, PdfViewComponent& comp) = 0;

        /** Disegna il testo/grafica mostrata quando nessun documento e' caricato. */
        virtual void drawPdfViewNoDocument (juce::Graphics& g, int width, int height, PdfViewComponent& comp) = 0;

        /** Disegna l'ombra attorno alla pagina PDF. */
        virtual void drawPdfViewPageShadow (juce::Graphics& g, juce::Rectangle<float> pageBounds, PdfViewComponent& comp) = 0;
    };

    //==============================================================================
    /// Listener per gli eventi semantici del viewer.
    class Listener
    {
    public:
        virtual ~Listener() = default;

        /** Chiamato quando l'utente naviga a una nuova pagina. @param newPage  Numero pagina 1-based. */
        virtual void pdfPageChanged (PdfViewComponent* comp, int newPage) = 0;

        /** Chiamato quando un documento viene caricato con successo. */
        virtual void pdfDocumentLoaded (PdfViewComponent* comp) {}

        /** Chiamato quando il documento viene chiuso. */
        virtual void pdfDocumentClosed (PdfViewComponent* comp) {}
    };

    //==============================================================================
    PdfViewComponent();
    ~PdfViewComponent() override;

    //==============================================================================
    // API BACKWARD COMPATIBLE — stessa firma di PDFComponent

    /** Carica un documento PDF dal percorso specificato.
     *  @param filePath  Percorso assoluto al file PDF.
     */
    void loadDocument (const juce::String& filePath);

    /** Ritorna true se un documento e' attualmente caricato. */
    [[nodiscard]] bool thereIsADocumentLoaded() const;

    /** Ritorna il numero totale di pagine del documento corrente (0 se nessun doc). */
    [[nodiscard]] int getTotPagesNum() const;

    /** Ritorna il numero di pagina attualmente visualizzata (1-based). */
    [[nodiscard]] int getCurrentPageOnScreen() const;

    /** Naviga alla pagina specificata (1-based). */
    void setPageNumber (int pageNumber);

    /** Ritorna la larghezza del documento in punti PDF. */
    [[nodiscard]] float getDocumentWidth() const;

    /** Ritorna l'altezza del documento in punti PDF. */
    [[nodiscard]] float getDocumentHeight() const;

    /** Ritorna il fattore di zoom corrente. */
    [[nodiscard]] float getCurrentPageZoom() const;

    /** Imposta il fattore di zoom mantenendo il punto di handle fisso sullo schermo.
     *  @param zoom         Nuovo fattore di zoom (1.0 = dimensione nativa).
     *  @param handlePoint  Punto in coordinate widget attorno a cui centrare lo zoom.
     */
    void setCurrentPageZoom (float zoom, juce::Point<float> handlePoint);

    /** Overload convenienza con coordinate int. */
    inline void setCurrentPageZoom (float zoom, juce::Point<int> handlePoint)
    {
        setCurrentPageZoom (zoom, handlePoint.toFloat());
    }

    /** Ritorna la posizione corrente del top-left della pagina in coordinate widget. */
    [[nodiscard]] juce::Point<float> getCurrentPageTopLeftPosition() const;

    /** Imposta la posizione del top-left della pagina in coordinate widget. */
    void setCurrentPageTopLeftPosition (juce::Point<float> newPos);

    /** Overload convenienza con coordinate int. */
    inline void setCurrentPageTopLeftPosition (juce::Point<int> newPos)
    {
        setCurrentPageTopLeftPosition (newPos.toFloat());
    }

    /** Ritorna il bounding rect della pagina corrente in coordinate widget. */
    [[nodiscard]] juce::Rectangle<float> getCurrentPageBounds() const;

    /** Esporta il documento corrente come file PDF.
     *  @param withName          Nome del file senza estensione.
     *  @param folderPath        Percorso cartella di destinazione.
     */
    void exportCurrentDocument (const juce::String& withName, const juce::String& folderPath) const;

    /** Carica un documento PDF da buffer in memoria. */
    void loadDocumentFromMemoryBlock (const void* data, int sizeInBytes);

    /** Serializza il documento corrente in un MemoryBlock. */
    void getMemoryBlockFromDocument (juce::MemoryBlock& destData);

    //==============================================================================
    // NUOVA API

    /** Collega un documento esterno gia' caricato.
     *  Il viewer NON prende ownership — il documento deve rimanere in vita finche'
     *  il viewer e' in uso o finche' non viene chiamata loadDocument()/loadDocumentFromMemoryBlock().
     */
    void setDocument (PdfDocument& doc);

    //==============================================================================
    // EVENTI — doppia API Listener + std::function

    void addListener    (Listener* l);
    void removeListener (Listener* l);

    std::function<void (int)> onPageChanged;    ///< Chiamata con il nuovo numero pagina (1-based)
    std::function<void()>     onDocumentLoaded; ///< Chiamata quando il documento e' caricato
    std::function<void()>     onDocumentClosed; ///< Chiamata quando il documento viene chiuso

    //==============================================================================
    void paint   (juce::Graphics& g) override;
    void resized ()                  override;

private:
    //==============================================================================
    /** Accesso al LookAndFeel con fallback automatico al default Ayra. */
    LookAndFeelMethods& getLAF();

    PdfDocument  ownedDocument;                 ///< Documento posseduto dal widget (loadDocument/loadFromMemory)
    PdfDocument* currentDocument { nullptr };   ///< Puntatore al documento attivo (owned o esterno)

    float                     currentZoom { 1.0f };
    juce::Point<float>        topLeft     {};
    int                       currentPage { 1 };

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PdfViewComponent)
};

} // namespace ayra
