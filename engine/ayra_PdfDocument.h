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

class PdfRenderer;

/** Engine principale per la lettura e manipolazione di documenti PDF.
 *
 *  Facade cross-platform: istanzia automaticamente il renderer corretto
 *  per la piattaforma corrente:
 *  - macOS / iOS  -> MacPdfRenderer (CoreGraphics, zero dipendenze esterne)
 *  - Windows      -> PdfiumRenderer (PDFium Apache 2.0)
 *  - Linux        -> PdfiumRenderer (PDFium Apache 2.0)
 *  - Android      -> PdfiumRenderer (PDFium Apache 2.0)
 *
 *  Progettato per uso headless: nessuna dipendenza su JUCE GUI. Usabile
 *  da server-side, CI, agenti AI e render offline senza finestra aperta.
 *
 *  ### Esempio di utilizzo
 *  @code
 *  ayra::PdfDocument doc;
 *  if (doc.open (juce::File ("/path/to/file.pdf")))
 *  {
 *      int pages = doc.getPageCount();
 *      juce::Image img = doc.renderPage (0, 2.0f);   // pagina 1, zoom 2x
 *      juce::String text = doc.extractText (0);
 *  }
 *  @endcode
 *
 *  @see MacPdfRenderer, PdfiumRenderer, PdfViewComponent
 */
class PdfDocument
{
public:
    PdfDocument();
    ~PdfDocument();

    //==============================================================================
    // CARICAMENTO / SALVATAGGIO

    /** Apre un documento PDF da file.
     *  @return true se il documento e' stato caricato correttamente.
     *  @note Idempotente: chiude automaticamente il documento precedente.
     */
    [[nodiscard]] bool open (const juce::File& file) noexcept;

    /** Apre un documento PDF da buffer in memoria.
     *  @param data       Puntatore ai byte del PDF (non deve essere null).
     *  @param sizeBytes  Dimensione del buffer in byte.
     *  @return true se il documento e' stato caricato correttamente.
     */
    [[nodiscard]] bool open (const void* data, size_t sizeBytes) noexcept;

    /** Salva il documento corrente su file.
     *  @return true se il salvataggio e' riuscito.
     *  @pre isOpen() == true
     */
    [[nodiscard]] bool save (const juce::File& destFile) const noexcept;

    /** Serializza il documento corrente in un MemoryBlock.
     *  @return true se la serializzazione e' riuscita.
     *  @pre isOpen() == true
     */
    [[nodiscard]] bool saveToMemoryBlock (juce::MemoryBlock& destData) const noexcept;

    /** Chiude il documento e rilascia le risorse del renderer.
     *  Dopo questa chiamata isOpen() ritorna false.
     */
    void close() noexcept;

    //==============================================================================
    // STATO

    /** Ritorna true se un documento e' attualmente aperto. */
    [[nodiscard]] bool isOpen() const noexcept;

    //==============================================================================
    // PAGINE

    /** Ritorna il numero totale di pagine del documento.
     *  @return Numero pagine, oppure 0 se nessun documento e' aperto.
     */
    [[nodiscard]] int getPageCount() const noexcept;

    /** Ritorna le informazioni geometriche di una pagina.
     *  @param pageIndex  Indice 0-based della pagina.
     *  @return PdfPage con bounds e rotazione, oppure PdfPage{} se l'indice e' fuori range.
     */
    [[nodiscard]] PdfPage getPage (int pageIndex) const noexcept;

    //==============================================================================
    // RENDERING

    /** Renderizza una pagina come immagine JUCE.
     *
     *  @param pageIndex  Indice 0-based della pagina da renderizzare.
     *  @param scale      Fattore di scala (1.0f = dimensioni native in punti PDF, 2.0f = doppia risoluzione).
     *  @return Immagine ARGB renderizzata, oppure immagine invalida se il rendering fallisce.
     *
     *  @note Non chiamare nel processBlock: puo' allocare memoria.
     */
    [[nodiscard]] juce::Image renderPage (int pageIndex, float scale = 1.0f) noexcept;

    //==============================================================================
    // RICERCA E ESTRAZIONE TESTO

    /** Cerca un testo nel documento.
     *
     *  @param query      Stringa da cercare (case-sensitive dipende dall'implementazione).
     *  @param pageIndex  Pagina in cui cercare (0-based), oppure -1 per cercare in tutto il doc.
     *  @return Array di PdfSearchResult con le posizioni dei match trovati.
     */
    [[nodiscard]] juce::Array<PdfSearchResult> findText (const juce::String& query, int pageIndex = -1) noexcept;

    /** Estrae il testo grezzo da una pagina.
     *  @param pageIndex  Indice 0-based della pagina.
     *  @return Testo estratto, oppure stringa vuota se la pagina non contiene testo selezionabile.
     */
    [[nodiscard]] juce::String extractText (int pageIndex) noexcept;

    //==============================================================================
    // ACCESSO AL RENDERER (avanzato)

    /** Ritorna un puntatore al renderer interno per operazioni avanzate.
     *  @return Puntatore al renderer, o nullptr se non ancora inizializzato.
     *  @note Non usare dall'audio thread.
     */
    [[nodiscard]] PdfRenderer* getRenderer() noexcept;

private:
    std::unique_ptr<PdfRenderer> renderer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PdfDocument)
};

} // namespace ayra
