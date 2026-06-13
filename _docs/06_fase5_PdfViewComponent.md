# Fase 5 — PdfViewComponent (Widget)

**Autore**: Ayra Soft  
**Data creazione**: 2026-05-26  
**Stato**: in attesa (dipende da Fase 4)

---

## Obiettivo

Implementare `widgets/ayra_PdfViewComponent.cpp` — il widget JUCE completo che usa `PdfDocument`
per visualizzare, navigare e interagire con il PDF. Sostituire `PDFComponent` legacy.

Deliverable:
- Rendering della pagina corrente via cache `juce::Image`
- Zoom corretto con anchor point fisso (fix del bug v1)
- Pan con mouse drag e scroll wheel
- Pinch gesture su trackpad macOS
- Navigazione pagine
- Evidenziazione risultati di ricerca (overlay)
- Backward compat: tutte le firme API di `PDFComponent` implementate

---

## Dipendenze

- Fase 4 (PdfDocument) deve essere completa
- `PdfDefaultLookAndFeel` deve essere presente (gia' in Fase 1)

---

## Rendering in paint()

Il widget mantiene una cache `juce::Image` della pagina corrente. La cache viene invalidata
quando cambia la pagina o il documento, e rasterizzata on-demand al primo `paint()` successivo.

```cpp
void PdfViewComponent::paint (juce::Graphics& g)
{
    auto& laf = getLAF();
    laf.drawPdfViewBackground (g, getWidth(), getHeight(), *this);

    if (currentDocument == nullptr || !currentDocument->isOpen())
    {
        laf.drawPdfViewNoDocument (g, getWidth(), getHeight(), *this);
        return;
    }

    // Rasterizza la pagina on-demand (risoluzione HiDPI)
    if (!cachedPageImage.isValid())
    {
        // Scale a risoluzione display (retina = 2.0)
        cachedPageImage = currentDocument->renderPage (currentPage - 1, currentScale * currentZoom);
    }

    if (cachedPageImage.isValid())
    {
        auto pageBounds = getCurrentPageBounds();
        laf.drawPdfViewPageShadow (g, pageBounds, *this);

        // L'immagine e' gia' alla risoluzione corretta (renderizzata a currentScale * currentZoom).
        // La disegniamo alla dimensione fisica (w/h del componente * zoom) senza ulteriore scaling.
        g.drawImageAt (cachedPageImage, (int)topLeft.x, (int)topLeft.y);
    }
}
```

**Strategia di caching**: l'immagine viene rasterizzata alla risoluzione `currentScale * currentZoom`.
Questo significa che ad ogni cambio di zoom, la cache viene invalidata e re-rasterizzata.
E' piu' semplice di una strategia "rasterizza a risoluzione massima e scala", ma causa
un rendering leggermente meno nitido durante il drag-zoom. Accettabile per la prima implementazione.

---

## Cache invalidata da

```cpp
// Campo privato da aggiungere in ayra_PdfViewComponent.h:
// juce::Image cachedPageImage;
// float       currentScale { 1.0f }; // DPI scale dal display corrente

void invalidatePageCache()
{
    cachedPageImage = {};
    repaint();
}
```

La cache va invalidata in questi metodi:
- `loadDocument()` — nuovo documento
- `loadDocumentFromMemoryBlock()` — nuovo documento
- `setDocument()` — documento esterno collegato (gia' chiama `repaint()`)
- `setPageNumber()` — cambia pagina
- `setCurrentPageZoom()` — cambia zoom
- `resized()` — cambia scala DPI o dimensione componente

---

## Scale HiDPI

```cpp
void PdfViewComponent::resized()
{
    // Aggiorna la scala DPI per il display corrente
    auto* display = juce::Desktop::getInstance()
        .getDisplays().getDisplayForRect (getScreenBounds());
    if (display != nullptr)
        currentScale = (float)display->scale;

    invalidatePageCache();
}
```

---

## Gesture via mouse events JUCE

```cpp
void PdfViewComponent::mouseDown (const juce::MouseEvent& e)
{
    lastDragPos = e.position;
}

void PdfViewComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isLeftButtonDown())
    {
        topLeft += e.position - lastDragPos;
        clampTopLeft();
        // Pan non invalida la cache (l'immagine si sposta, non si rasterizza di nuovo)
        repaint();
    }
    lastDragPos = e.position;
}

void PdfViewComponent::mouseWheelMove (const juce::MouseEvent& e,
                                       const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown())
    {
        // Ctrl/Cmd + scroll = zoom
        float newZoom = juce::jlimit (0.1f, 10.0f, currentZoom + w.deltaY * 0.15f);
        setCurrentPageZoom (newZoom, e.position);
    }
    else
    {
        // Scroll normale = pan
        topLeft += juce::Point<float> (w.deltaX * 30.f, w.deltaY * 30.f);
        clampTopLeft();
        repaint();
    }
}

void PdfViewComponent::mouseMagnify (const juce::MouseEvent& e, float magnifyAmount)
{
    // Pinch trackpad macOS
    float newZoom = juce::jlimit (0.1f, 10.0f, currentZoom * magnifyAmount);
    setCurrentPageZoom (newZoom, e.position);
}
```

---

## setCurrentPageZoom — fix anchor point

Sostituzione della formula errata di v1. L'anchor viene convertito da viewport-space
a PDF-space prima di applicare il nuovo zoom.

```cpp
void PdfViewComponent::setCurrentPageZoom (float newZoom, juce::Point<float> handle)
{
    const float oldZoom = currentZoom;
    if (juce::approximatelyEqual (oldZoom, newZoom)) { return; }

    // Converti anchor da viewport-space a PDF-space (unita' punti / oldZoom)
    const float anchorPdfX = (handle.x - topLeft.x) / oldZoom;
    const float anchorPdfY = (handle.y - topLeft.y) / oldZoom;

    // Calcola il nuovo topLeft che mantiene anchor fisso in viewport
    topLeft.x = handle.x - anchorPdfX * newZoom;
    topLeft.y = handle.y - anchorPdfY * newZoom;

    currentZoom = newZoom;
    clampTopLeft();
    invalidatePageCache(); // rasterizza alla nuova risoluzione
}
```

---

## clampTopLeft — evita che la pagina sparisca fuori schermo

```cpp
void PdfViewComponent::clampTopLeft()
{
    if (currentDocument == nullptr || !currentDocument->isOpen()) { return; }

    auto page = currentDocument->getPage (currentPage - 1);
    if (!page.isValid()) { return; }

    const float docW = page.bounds.getWidth()  * currentZoom;
    const float docH = page.bounds.getHeight() * currentZoom;
    const float viewW = (float)getWidth();
    const float viewH = (float)getHeight();

    // Lascia almeno il 10% della pagina visibile
    const float minVisibleFraction = 0.1f;
    const float maxX =  viewW * (1.f - minVisibleFraction);
    const float maxY =  viewH * (1.f - minVisibleFraction);
    const float minX = -docW + viewW * minVisibleFraction;
    const float minY = -docH + viewH * minVisibleFraction;

    topLeft.x = juce::jlimit (minX, maxX, topLeft.x);
    topLeft.y = juce::jlimit (minY, maxY, topLeft.y);
}
```

---

## setPageNumber

```cpp
void PdfViewComponent::setPageNumber (int pageNumber)
{
    if (currentDocument == nullptr || !currentDocument->isOpen()) { return; }

    const int totalPages = currentDocument->getPageCount();
    if (pageNumber < 1 || pageNumber > totalPages) { return; }
    if (pageNumber == currentPage) { return; }

    currentPage = pageNumber;
    topLeft     = {};    // reset pan a inizio pagina
    invalidatePageCache();

    listeners.call ([this] (Listener& l) { l.pdfPageChanged (this, currentPage); });
    if (onPageChanged) { onPageChanged (currentPage); }
}
```

---

## loadDocument e loadDocumentFromMemoryBlock

```cpp
void PdfViewComponent::loadDocument (const juce::String& filePath)
{
    if (!ownedDocument.open (juce::File (filePath))) { return; }

    currentDocument = &ownedDocument;
    currentPage     = 1;
    currentZoom     = 1.0f;
    topLeft         = {};
    invalidatePageCache();

    listeners.call ([this] (Listener& l) { l.pdfDocumentLoaded (this); });
    if (onDocumentLoaded) { onDocumentLoaded(); }
}

void PdfViewComponent::loadDocumentFromMemoryBlock (const void* data, int sizeInBytes)
{
    if (!ownedDocument.open (data, (size_t)sizeInBytes)) { return; }

    currentDocument = &ownedDocument;
    currentPage     = 1;
    currentZoom     = 1.0f;
    topLeft         = {};
    invalidatePageCache();

    listeners.call ([this] (Listener& l) { l.pdfDocumentLoaded (this); });
    if (onDocumentLoaded) { onDocumentLoaded(); }
}
```

---

## getCurrentPageBounds

Le bounds vengono calcolate in screen-space (coordinate widget) applicando zoom e topLeft:

```cpp
juce::Rectangle<float> PdfViewComponent::getCurrentPageBounds() const
{
    if (currentDocument == nullptr || !currentDocument->isOpen()) { return {}; }

    auto page = currentDocument->getPage (currentPage - 1);
    if (!page.isValid()) { return {}; }

    return { topLeft.x, topLeft.y,
             page.bounds.getWidth()  * currentZoom,
             page.bounds.getHeight() * currentZoom };
}
```

---

## Attivazione alias backward compat (fine Fase 5)

Una volta che `PdfViewComponent` e' completamente implementato e testato:

**In `ayra_pdf.h`:**
```cpp
// 1. Rimuovere:
#include "pdf_component/PDFComponent.h"

// 2. Decommentare:
namespace ayra { using PDFComponent = PdfViewComponent; }
```

**In `ayra_pdf.cpp`:**
```cpp
// Rimuovere dal blocco macOS:
#include "pdf_component/Mac_PDF_core/PDFView.m"
#include "pdf_component/Mac_PDF_core/MacPDFComponent.mm"

// Rimuovere dal blocco widgets:
#include "pdf_component/PDFComponent.cpp"  // LEGACY backward compat
```

---

## Rimozione legacy (Fase 6)

Dopo aver verificato che tutto il codice che usava `PDFComponent` compila e funziona
con l'alias `using PDFComponent = PdfViewComponent`:

1. Eliminare la cartella `pdf_component/` intera
2. Verificare che nessun file in altri moduli o nell'app includa direttamente `PDFComponent.h`
   (deve fallire con errore di compilazione chiaro, non silenzioso)
3. Rimuovere `#include <Cocoa/Cocoa.h>` da `ayra_pdf.cpp` se non piu' necessario
   (il rendering CoreGraphics tramite `MacPdfRenderer` richiede solo `CoreGraphics.h`)

---

## Checklist Fase 5

- [ ] Aggiungere `juce::Image cachedPageImage` come membro privato in `ayra_PdfViewComponent.h`
- [ ] Aggiungere `float currentScale { 1.0f }` come membro privato
- [ ] Aggiungere `juce::Point<float> lastDragPos {}` come membro privato
- [ ] Implementare `invalidatePageCache()` — helper privato
- [ ] Implementare `clampTopLeft()` — helper privato
- [ ] Implementare `paint()` con cache e LookAndFeel
- [ ] Implementare `resized()` con aggiornamento DPI scale
- [ ] Implementare `loadDocument()` con ownedDocument e notifica listeners
- [ ] Implementare `loadDocumentFromMemoryBlock()` — analogo a loadDocument
- [ ] Implementare `setPageNumber()` con reset pan e notifica listeners
- [ ] Implementare `setCurrentPageZoom()` con formula corretta anchor point (FIX v1)
- [ ] Implementare `setCurrentPageTopLeftPosition()` con clamp
- [ ] Implementare `getCurrentPageBounds()` in screen-space
- [ ] Implementare `getDocumentWidth()` e `getDocumentHeight()`
- [ ] Implementare `thereIsADocumentLoaded()` -> `currentDocument->isOpen()`
- [ ] Implementare `getTotPagesNum()` -> `currentDocument->getPageCount()`
- [ ] Implementare `exportCurrentDocument()` -> `currentDocument->save(...)`
- [ ] Implementare `getMemoryBlockFromDocument()` -> `currentDocument->saveToMemoryBlock(...)`
- [ ] Implementare `mouseDown`, `mouseDrag`, `mouseWheelMove`, `mouseMagnify`
- [ ] Implementare `setDocument()` (gia' parzialmente presente)
- [ ] Verificare che `addListener`/`removeListener` funzionino
- [ ] Attivare alias `using PDFComponent = PdfViewComponent` in `ayra_pdf.h`
- [ ] Test macOS: aprire file, navigare pagine, zoom pinch/scroll, pan drag
- [ ] Test backward compat: codice che usava `PDFComponent` compila senza modifiche
