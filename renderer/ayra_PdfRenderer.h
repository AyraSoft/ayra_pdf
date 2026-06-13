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

/** Interfaccia pura per il rendering di documenti PDF.
 *
 *  Due implementazioni concrete:
 *  - MacPdfRenderer  : CoreGraphics nativo (macOS/iOS, zero dipendenze esterne)
 *  - PdfiumRenderer  : PDFium Apache 2.0  (Windows, Linux, Android)
 *
 *  PdfDocument possiede un'istanza concreta e la espone tramite getRenderer()
 *  per operazioni avanzate. Il consumer tipico usa direttamente PdfDocument
 *  senza toccare il renderer.
 *
 *  @see PdfDocument, MacPdfRenderer, PdfiumRenderer
 */
class PdfRenderer
{
public:
    virtual ~PdfRenderer() = default;

    //==============================================================================
    // CARICAMENTO / SALVATAGGIO

    /** Carica un documento PDF da file.
     *  @return true se il caricamento e' riuscito.
     */
    [[nodiscard]] virtual bool loadFromFile (const juce::File& file) noexcept = 0;

    /** Carica un documento PDF da buffer in memoria.
     *  @param data       Puntatore ai byte del PDF.
     *  @param sizeBytes  Dimensione del buffer in byte.
     *  @return true se il caricamento e' riuscito.
     */
    [[nodiscard]] virtual bool loadFromMemory (const void* data, size_t sizeBytes) noexcept = 0;

    /** Salva il documento corrente su file.
     *  @return true se il salvataggio e' riuscito.
     */
    [[nodiscard]] virtual bool saveToFile (const juce::File& destFile) const noexcept = 0;

    /** Serializza il documento corrente in un MemoryBlock.
     *  @return true se la serializzazione e' riuscita.
     */
    [[nodiscard]] virtual bool saveToMemory (juce::MemoryBlock& destData) const noexcept = 0;

    /** Chiude il documento e rilascia le risorse native. */
    virtual void close() noexcept = 0;

    //==============================================================================
    // STATO

    /** Ritorna true se un documento e' attualmente caricato. */
    [[nodiscard]] virtual bool isLoaded() const noexcept = 0;

    //==============================================================================
    // PAGINE

    /** Ritorna il numero totale di pagine. */
    [[nodiscard]] virtual int getPageCount() const noexcept = 0;

    /** Ritorna le informazioni geometriche di una pagina (0-based). */
    [[nodiscard]] virtual PdfPage getPage (int pageIndex) const noexcept = 0;

    //==============================================================================
    // RENDERING

    /** Renderizza una pagina come immagine JUCE ARGB.
     *  @param pageIndex  Indice 0-based della pagina.
     *  @param scale      Fattore di scala (1.0f = risoluzione nativa in punti PDF).
     *  @return Immagine renderizzata oppure immagine invalida in caso di errore.
     */
    [[nodiscard]] virtual juce::Image renderPage (int pageIndex, float scale) noexcept = 0;

    //==============================================================================
    // RICERCA E ESTRAZIONE TESTO

    /** Cerca un testo nel documento o in una singola pagina.
     *  @param query      Testo da cercare.
     *  @param pageIndex  Pagina target (0-based), oppure -1 per cercare in tutto il documento.
     *  @return Array di PdfSearchResult con le occorrenze trovate.
     */
    [[nodiscard]] virtual juce::Array<PdfSearchResult> findText (const juce::String& query, int pageIndex) noexcept = 0;

    /** Estrae il testo grezzo da una pagina (0-based). */
    [[nodiscard]] virtual juce::String extractText (int pageIndex) noexcept = 0;
};

} // namespace ayra
