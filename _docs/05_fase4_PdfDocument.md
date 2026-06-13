# Fase 4 — PdfDocument (Engine Facade)

**Autore**: Ayra Soft  
**Data creazione**: 2026-05-26  
**Stato**: in attesa (dipende da Fase 2 e Fase 3)

---

## Obiettivo

Implementare `engine/ayra_PdfDocument.cpp` sostituendo tutti i `jassertfalse` + TODO con
il codice reale che istanzia il renderer corretto e delega ogni metodo.

`PdfDocument` non ha logica propria: e' una factory + facade. Tutta la logica di rendering
vive nei renderer concreti.

**Dipendenze**: Fase 4 puo' essere completata parzialmente — ad esempio attivare solo
`MacPdfRenderer` su macOS mentre `PdfiumRenderer` e' ancora stub, e viceversa.

---

## Factory a compile-time

```cpp
PdfDocument::PdfDocument()
{
#if JUCE_MAC || JUCE_IOS
    renderer = std::make_unique<MacPdfRenderer>();
#elif JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID
    renderer = std::make_unique<PdfiumRenderer>();
#else
    #error "ayra_pdf: piattaforma non supportata. Aggiungere un backend per questa piattaforma."
#endif
}
```

Il distruttore restera' `= default`: `unique_ptr` distrugge automaticamente il renderer concreto.

```cpp
PdfDocument::~PdfDocument() = default;
```

---

## Delega di tutti i metodi

Pattern uniforme: guard su `renderer != nullptr` + delega. Nella pratica il renderer e' sempre
inizializzato dopo il costruttore, ma la guard protegge da usi anomali (es. dopo move, se
in futuro si aggiunge move semantics).

```cpp
bool PdfDocument::open (const juce::File& file) noexcept
{
    return renderer && renderer->loadFromFile (file);
}

bool PdfDocument::open (const void* data, size_t sizeBytes) noexcept
{
    return renderer && renderer->loadFromMemory (data, sizeBytes);
}

bool PdfDocument::save (const juce::File& destFile) const noexcept
{
    return renderer && renderer->saveToFile (destFile);
}

bool PdfDocument::saveToMemoryBlock (juce::MemoryBlock& destData) const noexcept
{
    if (!renderer) { return false; }
    return renderer->saveToMemory (destData);
}

void PdfDocument::close() noexcept
{
    if (renderer) { renderer->close(); }
}

bool PdfDocument::isOpen() const noexcept
{
    return renderer && renderer->isLoaded();
}

int PdfDocument::getPageCount() const noexcept
{
    return renderer ? renderer->getPageCount() : 0;
}

PdfPage PdfDocument::getPage (int pageIndex) const noexcept
{
    if (!renderer) { return {}; }
    return renderer->getPage (pageIndex);
}

juce::Image PdfDocument::renderPage (int pageIndex, float scale) noexcept
{
    if (!renderer) { return {}; }
    return renderer->renderPage (pageIndex, scale);
}

juce::Array<PdfSearchResult> PdfDocument::findText (const juce::String& query, int pageIndex) noexcept
{
    if (!renderer) { return {}; }
    return renderer->findText (query, pageIndex);
}

juce::String PdfDocument::extractText (int pageIndex) noexcept
{
    if (!renderer) { return {}; }
    return renderer->extractText (pageIndex);
}

PdfRenderer* PdfDocument::getRenderer() noexcept
{
    return renderer.get();
}
```

---

## Gestione renderer null

Il renderer viene sempre creato nel costruttore tramite `make_unique`. Le situazioni in cui
`renderer` potrebbe essere `nullptr` sono:

1. **Mai in uso normale**: il costruttore sempre inizializza il renderer.
2. **Dopo move** (se in futuro si implementa `PdfDocument(PdfDocument&&)`): il renderer dell'oggetto
   sorgente viene trasferito, lasciando `renderer = nullptr` nell'originale.
3. **Piattaforma non supportata**: la `#error` impedisce la compilazione, mai runtime.

La guard `renderer && ...` nei metodi e' quindi difensiva, non necessaria per il codice corrente.
Mantenerla per robustezza futura (es. se si aggiunge move semantics o factory method che puo'
ritornare un `PdfDocument` con renderer null come sentinel di errore).

---

## Rimozione dei jassertfalse

Il file corrente ha tutti i metodi che fanno `jassertfalse; return false/{}`. Quando si
implementa la Fase 4:

1. Rimuovere il blocco `#if JUCE_MAC || JUCE_IOS` / `#else` / `#endif` commentato dal costruttore.
2. Sostituire ogni `jassertfalse; return false;` con la delega corrispondente.
3. Verificare che le inclusioni dei renderer concreti siano corrette in `ayra_pdf.cpp`.

---

## Inclusioni in ayra_pdf.cpp

Per far compilare la Fase 4, `ayra_pdf.cpp` deve includere i renderer concreti **prima**
dell'engine. L'ordine attuale e' gia' corretto:

```cpp
// ayra_pdf.cpp — ordine corrente (corretto)
#if JUCE_MAC || JUCE_IOS
  #include <CoreGraphics/CoreGraphics.h>
  #include <Cocoa/Cocoa.h>
  #include "pdf_component/Mac_PDF_core/PDFView.m"          // LEGACY (rimuovere in Fase 6)
  #include "pdf_component/Mac_PDF_core/MacPDFComponent.mm" // LEGACY (rimuovere in Fase 6)
  #include "renderer/mac/ayra_MacPdfRenderer.mm"           // Fase 2

#elif JUCE_WINDOWS
  #include "renderer/pdfium/ayra_PdfiumRenderer.cpp"       // Fase 3

#elif JUCE_ANDROID || JUCE_LINUX
  #include "renderer/pdfium/ayra_PdfiumRenderer.cpp"       // Fase 3
#endif

#include "engine/ayra_PdfDocument.cpp"                     // Fase 4 — DEVE venire dopo i renderer
```

Questo ordine garantisce che quando `ayra_PdfDocument.cpp` viene compilato, i tipi
`MacPdfRenderer` e `PdfiumRenderer` siano gia' definiti.

---

## Attivazione parziale per piattaforma

E' possibile attivare la Fase 4 per macOS prima che la Fase 3 (PdfiumRenderer) sia completa:

```cpp
// ayra_PdfDocument.cpp — versione di transizione
PdfDocument::PdfDocument()
{
#if JUCE_MAC || JUCE_IOS
    renderer = std::make_unique<MacPdfRenderer>(); // Fase 2 completa
#elif JUCE_WINDOWS || JUCE_LINUX || JUCE_ANDROID
    renderer = std::make_unique<PdfiumRenderer>(); // Fase 3 in progress — stub
#else
    #error "piattaforma non supportata"
#endif
}
```

Con questa configurazione, su macOS tutto funziona (renderer concreto), su Win/Linux i
metodi ritornano `false` / immagine invalida (stub del PdfiumRenderer) senza `jassertfalse`
(che e' solo in Debug). E' una situazione accettabile per sviluppo incrementale.

---

## Checklist Fase 4

- [ ] Attendere che Fase 2 (MacPdfRenderer) sia completa
- [ ] Attendere che Fase 3 (PdfiumRenderer) sia completa (o accettare attivazione parziale)
- [ ] Rimuovere costruttore commentato e sostituire con factory attiva
- [ ] Implementare tutti i metodi di delega (vedi sezione sopra)
- [ ] Rimuovere tutti i `jassertfalse` dai metodi di PdfDocument
- [ ] Verificare inclusione renderers in `ayra_pdf.cpp` (gia' nell'ordine corretto)
- [ ] Test macOS: `PdfDocument doc; doc.open(file); doc.renderPage(0, 1.0f)` produce immagine valida
- [ ] Test Win/Linux: stesso test con PdfiumRenderer
- [ ] Test headless: compilare con `AYRA_PDF_HEADLESS` e usare `PdfDocument` senza widget
