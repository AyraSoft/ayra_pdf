# Stato Attuale — ayra_pdf

**Autore**: Ayra Soft  
**Data aggiornamento**: 2026-10-02

---

> **AUTORITA' DEL DOCUMENTO** — Questo file e' la **Single Source of Truth** per stato,
> priorita', dipendenze e avanzamento del refactoring `ayra_pdf`. I documenti
> `03_fase2_...` ... `06_fase5_...` sono specifiche tecniche di area e NON possiedono
> lo stato operativo. `00_storico.md` e' provenance storica.
>
> **Guideline normativa** — Ogni modifica segue
> `AyraSoft/ayra_multiverse/_doc/AYRA_AI_ENGINEERING_MASTER.md`: correctness, SSoT,
> module-first, current-only, minimal architectural diff, repository safety e verification gate.
>
> **Limite di questa sessione** — si possono leggere/scrivere codice e documentazione,
> ma **non si possono lanciare build, test binari o CI**. Ogni microstep distingue quindi
> `static-audited` da `externally-verified`.

## Baseline verificata

Ricognizione effettuata su `main@fc60d44e325265b85a1616205157ee871d95350a` prima
dell'avvio dell'implementazione del 2026-10-02.

- L'architettura v2 (engine / renderer / widget) esiste come skeleton.
- Il solo path legacy operativo e' macOS/AppKit (`AyraLegacyPDFView` deriva da `NSView`); non esiste un legacy iOS funzionante.
- `MacPdfRenderer` M01-M04 e' implementato staticamente; `PdfiumRenderer`, `PdfDocument` e gran parte di `PdfViewComponent` hanno ancora lavoro pending.
- Code search AyraSoft non ha trovato consumer di `PDFComponent` fuori da questo repository;
  l'audit va ripetuto immediatamente prima del cutover.
- Il vecchio piano `using PDFComponent = PdfViewComponent` e' annullato: il Master impone current-only.
- `AYRA_PDF_HEADLESS` oggi esclude i widget, ma il modulo include/dichiara ancora `juce_gui_extra`;
  quindi il vecchio claim "zero dipendenze GUI" non e' attualmente vero.

## Roadmap canonica a microstep

| ID | Area | Deliverable | Dipendenze | Stato |
|---|---|---|---|---|
| **S00** | Governance/docs | Stato canonico, roadmap, current-only, limiti verifica | - | **done (static)** |
| **M01** | Mac renderer | Lifecycle, load file/memory, close, page count/metadata | S00 | **done (static)** |
| **M02** | Mac renderer | Rasterizzazione pagina -> `juce::Image` con validazione scale/size | M01 | **done (static)** |
| **M03** | Mac renderer | Save file + save memory byte-preserving | M01 | **done (static)** |
| **M04** | Mac renderer | Estrazione testo + ricerca PDFKit con bounds esatti | M01 | **done (static)** |
| **D01** | Engine | `PdfDocument` factory/delega attiva su Apple | M01-M04 | **done (static)** |
| **P01** | PDFium | Init/lifetime, load file/memory, close, metadata | S00 | **done (static)** |
| **P02** | PDFium | Render + save file/memory | P01 | **done (static)** |
| **P03** | PDFium | Extract/search con bounds esatti | P01 | **done (static)** |
| **D02** | Engine | `PdfDocument` completo su tutti i target dichiarati | D01,P01-P03 | **done (static)** |
| **W01** | Widget | Load/navigation/cache/render + notifiche | D02 | **in progress** |
| **W02** | Widget | Zoom anchor, pan, wheel/pinch, HiDPI, search overlay | W01 | pending |
| **H01** | Module contract | Rendere esplicito e corretto il contratto headless/dependencies JUCE | D02 | pending |
| **C01** | Current-only cutover | Rimuovere `pdf_component/`, include legacy e ogni alias/fallback | W02,H01 | pending |
| **V01** | Verification esterna | Build/test target dichiarati + sanitizer dove applicabile | C01 | external pending |

### Regole di avanzamento

1. Un microstep cambia solo il canonical owner della responsabilita' coinvolta.
2. Il legacy e' reference read-only, non una seconda implementazione da mantenere sincronizzata.
3. Una funzione completata perde il relativo `jassertfalse`/TODO; niente placeholder mascherati.
4. PDF, indici, size e path sono input non affidabili: validare prima di allocare/indicizzare.
5. Render/parse/save non sono realtime-safe e non vanno chiamati dal processBlock.
6. Nessun `PdfSearchResult` con bounds finti/vuoti viene dichiarato completo.
7. Fine microstep: diff audit statico su correctness, ownership, lifetime, scope, regressioni.
8. Build/test/CI rimangono **external pending** in questa sessione.

## Change Contract — M01 MacPdfRenderer lifecycle/load/metadata

**Task model**: rendere reale il possesso del documento CoreGraphics senza introdurre stato viewport/GUI.

**Canonical owner**: `renderer/mac/ayra_MacPdfRenderer.mm` possiede `CGPDFDocumentRef` e
l'eventuale backing memory necessaria per un documento aperto da buffer.

**Existing reusable path**: il legacy `MacPDFComponent.mm` e' solo riferimento read-only;
bug e ownership legacy non vengono copiati automaticamente.

**Expected files**:
- `renderer/mac/ayra_MacPdfRenderer.mm` — implementazione;
- `renderer/mac/ayra_MacPdfRenderer.h` — solo allineamento del contratto/documentazione;
- questo file — stato a fine microstep.

**Forbidden ownership changes**:
- nessun zoom/pan/current-page nel renderer;
- nessuna logica widget in engine/renderer;
- nessuna modifica a JUCE o third-party;
- nessun alias/compatibility layer.

**Invariants**:
- `close()` idempotente e unico punto di rilascio del documento;
- ogni successful load sostituisce deterministicamente il documento precedente;
- backing buffer del load-from-memory vivo almeno quanto il documento;
- API pagina 0-based, CoreGraphics 1-based confinato all'adapter;
- indice negativo/out-of-range controllato prima di cast unsigned;
- nessun leak di documento/provider; nessun accesso GUI.

**Acceptance criteria M01**:
- `loadFromFile`, `loadFromMemory`, `close`, `isLoaded`, `getPageCount`, `getPage` reali;
- nessun `jassertfalse` in tali funzioni;
- input invalidi non lasciano stato parzialmente caricato;
- `getPage` ritorna indice, bounds e rotazione coerenti;
- static audit completato; build/test **external pending**.

**Esito M01 (2026-10-02)**: implementati lifecycle transazionale, ownership esplicita
`CGPDFDocumentRef -> CGDataProviderRef -> backing MemoryBlock`, caricamento file/memoria,
conteggio e metadati pagina con range/narrowing checks. Nessuna build/CI eseguita qui.

---

## Esito M02 / M03

**M02 (2026-10-02)**: raster CoreGraphics diretto su `juce::SoftwareImageType`,
ARGB premultiplied compatibile col layout JUCE Apple, backdrop bianco, output top-down,
rotazioni PDF 0/90/180/270 gestite dal transform CoreGraphics. Input `scale`, dimensioni,
pixel count, stride e buffer size sono bounded/validati. Limiti canonici condivisi con il
futuro backend PDFium in `renderer/ayra_PdfSafetyLimits.h`.

**M03 (2026-10-02)**: `saveToFile` e `saveToMemory` sono byte-preserving tramite snapshot
del `CGDataProvider`; non ridisegnano le pagine e quindi non appiattiscono metadata,
annotations, form fields o struttura. Le destinazioni hanno semantica transazionale.
Build e round-trip runtime restano **external pending**.

## Change Contract — M04 text extraction / search Apple

**Task model**: fornire estrazione testo Unicode e ricerca con bounds reali senza implementare
un parser PDF/font/CMap proprietario.

**Canonical owner**:
- `MacPdfRenderer`: orchestration del backend Apple;
- CoreGraphics: lifecycle/raster;
- PDFKit: content model testuale e selezioni;
- `PdfSearchResult`: contratto geometrico cross-backend.

**Existing reusable path**: PDFKit `PDFDocument/PDFPage/PDFSelection`. Il vecchio design
`CGPDFScanner` manuale e' scartato perche' non copre in modo robusto Type0/CMap/XObject e
non garantisce bounds corretti.

**Expected files**:
- `ayra_pdf.h` — dichiarazione framework PDFKit per macOS/iOS;
- `renderer/mac/ayra_MacPdfRenderer.mm` — cache PDFKit derivata dai byte canonici;
- `engine/ayra_PdfSearchResult.h`, `PdfPage.h`, `PdfRenderer.h`, `PdfDocument.h` — contratto;
- documentazione corrente.

**Forbidden ownership changes**:
- nessun testo/search nel widget;
- nessun secondo buffer PDF editabile/autoritativo;
- nessun parser CMap/font custom;
- nessuna conversione in screen space nel renderer.

**Invariants**:
- il modello PDFKit e' una cache derivata dagli stessi byte del provider CoreGraphics;
- invalidazione deterministica in `close()`/nuovo load;
- `findText` e' case-insensitive su tutti i backend;
- bounds sempre 72 dpi, lower-left/Y-up; il widget converte una sola volta;
- `pageIndex` -1 = documento intero, altrimenti 0-based e validato prima di chiamare PDFKit;
- risultati senza bounds finite/non-empty non vengono pubblicati;
- parser/search non sono realtime-safe e l'istanza resta externally serialized;
- eccezioni native da PDF esterni malformati vengono tradotte in failure/risultato vuoto al boundary.

**Acceptance criteria M04**:
- `extractText` usa `PDFPage.string` e preserva Unicode;
- `findText` cerca per pagina su `PDFPage.string`, converte ogni range con `selectionForRange:` e usa `PDFSelection boundsForPage:`;
- nessun `jassertfalse` resta nei metodi Mac;
- nessun bounds placeholder;
- static diff audit completato;
- test runtime su UTF-8/UTF-16, Type0/CMap, multiline match, pagina filtrata e PDF malformato:
  **external pending**.

---

## Esito M04

**M04 (2026-10-02)**: il backend Apple usa PDFKit come content model derivato dai medesimi
byte del `CGDataProvider`. `extractText` usa `PDFPage.string`; `findText` e'
case-insensitive, page-bounded, converte i range tramite `selectionForRange:` e pubblica
solo bounds finite/non-empty in PDF page space. Query, testo per pagina e numero di match
sono bounded. Superato il budget risultati, la ricerca fallisce atomicamente: nessuna lista
troncata viene presentata come completa. La cache PDFKit viene invalidata prima di
document/provider/backing. Nel renderer Apple non restano `jassertfalse` o TODO.
Build/test runtime restano **external pending**.

### Nota piattaforma Apple

Il backend v2 e' scritto sotto `JUCE_MAC || JUCE_IOS` e usa framework disponibili su entrambe
le piattaforme. Il legacy invece e' AppKit/macOS-specifico (`AyraLegacyPDFView : NSView`): finche' C01
non rimuove quel percorso, l'integrazione iOS dell'intero modulo NON e' dichiarata verificata.

## Change Contract — D01 PdfDocument facade Apple

**Task model**: trasformare `PdfDocument` da skeleton in factory/facade reale, senza spostare
nel facade alcuna logica PDF.

**Canonical owner**:
- `PdfDocument`: ownership del `std::unique_ptr<PdfRenderer>` e sola delega;
- `MacPdfRenderer`: tutta la semantica Apple;
- `PdfiumRenderer`: semantica non-Apple, ancora pending P01-P03.

**Expected files**:
- `engine/ayra_PdfDocument.cpp`;
- documentazione D01;
- commenti di aggregazione in `ayra_pdf.cpp` solo se necessari.

**Forbidden ownership changes**:
- nessun parsing/raster/search nel facade;
- nessuna duplicazione di safety limit o policy;
- nessun fallback legacy;
- nessuna modifica a JUCE/third-party.

**Invariants**:
- factory compile-time: Apple -> `MacPdfRenderer`, Win/Linux/Android -> `PdfiumRenderer`;
- ogni metodo pubblico e' guard + delega, nessuna seconda source of truth;
- nessun `jassertfalse`/TODO nel facade;
- nessuna allocazione o chiamata PDF e' realtime-safe;
- D01 certifica staticamente il percorso Apple; D02 resta bloccato da P01-P03.

**Acceptance criteria D01**:
- costruttore istanzia il renderer corretto;
- open/save/close/state/page/render/search/extract delegano 1:1;
- `getRenderer()` resta accesso non-owning avanzato;
- diff audit statico completato;
- build/test macOS+iOS: **external pending**.

**Esito D01 (2026-10-02)**: `PdfDocument` e' ora una factory/facade reale e priva di
stub. Il diff contiene soltanto selezione compile-time del renderer e delega 1:1. Il
percorso Apple usa il `MacPdfRenderer` M01-M04; i target non-Apple restano funzionalmente
bloccati dal backend PDFium fino a P01-P03.

---

## Change Contract — P01 PDFium lifecycle / load / metadata

**Task model**: rendere reale il backend PDFium minimo, con lifecycle process-wide corretto,
un solo percorso canonico di caricamento e metadata pagina coerenti con il backend Apple.

**Evidenza threading**: l'header vendorizzato `fpdfview.h` dichiara esplicitamente che
**nessuna API PDFium e' thread-safe** e richiede all'embedder mutex/serializzazione globale.
Di conseguenza la sincronizzazione appartiene al backend, non ai consumer.

**Canonical owner**:
- `PdfiumRenderer.cpp`: init/destroy, mutex process-wide, documento e backing bytes;
- `renderer/ayra_PdfSafetyLimits.h`: limiti di input condivisi;
- PDFium: parsing e page metadata.

**Existing reusable path**:
- `FPDF_InitLibraryWithConfig` / `FPDF_DestroyLibrary`;
- `FPDF_LoadMemDocument64`;
- `FPDF_GetPageCount`, `FPDF_LoadPage`, `FPDFPage_GetMediaBox`,
  `FPDFPage_GetRotation`.

**Expected files**:
- `renderer/pdfium/ayra_PdfiumRenderer.cpp`;
- `renderer/pdfium/ayra_PdfiumRenderer.h`;
- `_docs/04_fase3_PdfiumRenderer.md`;
- questo documento.

**Forbidden ownership changes**:
- nessuna lock nel consumer/`PdfDocument`;
- nessun secondo file-loading path tramite `FPDF_LoadDocument`;
- nessun fallback silenzioso se PDFium non e' linkato;
- nessuna logica render/save/text di P02/P03 anticipata;
- nessuna modifica a header/vendor PDFium.

**Invariants**:
- init PDFium sul passaggio instance-count 0 -> 1, destroy su 1 -> 0, entrambi sotto
  la stessa lock process-wide che serializza tutte le API FPDF;
- nessun `once_flag` combinato con destroy/re-init;
- ogni futura chiamata PDFium deve acquisire la stessa lock process-wide process-wide;
- `loadFromFile` legge bounded con JUCE e converge su `FPDF_LoadMemDocument64`;
- backing bytes owned restano validi fino a `FPDF_CloseDocument`;
- load fallito lascia intatto il documento precedente;
- `close()` e' idempotente;
- page metadata usa MediaBox in PDF page space 72 dpi, lower-left/Y-up;
- `PdfiumRenderer` non e' realtime-safe; lifetime dell'istanza resta responsabilita' del caller.

**Acceptance criteria P01**:
- init/destroy process-wide senza race e senza doppio init/destroy;
- `loadFromFile`, `loadFromMemory`, `close`, `isLoaded`, `getPageCount`, `getPage`
  reali e senza `jassertfalse`;
- document size bounded prima dell'allocazione;
- metadata pagina finite/non-empty e rotation valida 0/90/180/270;
- nessun silent fallback `AYRA_PDFIUM_AVAILABLE`;
- static diff audit completato;
- build/test Win/Linux/Android e stress multi-thread: **external pending**.


**Esito P01 (2026-10-02)**: implementato lifecycle process-wide con
`juce::CriticalSection` e instance-count protetto; init 0->1 e destroy 1->0.
`loadFromFile` esegue lettura bounded via JUCE e converge su
`FPDF_LoadMemDocument64`; load file/memory sono transazionali e mantengono owned i
backing bytes. `close/isLoaded/getPageCount/getPage` sono reali; metadata usa MediaBox
72 dpi lower-left/Y-up e rotation PDFium. Tutte le chiamate FPDF attualmente attive sono
serializzate dalla stessa lock. Nessun fallback headers-missing resta nel backend.
Build/test Win/Linux/Android e stress concorrente: **external pending**.

---

## Change Contract — P02 PDFium save / raster

**Task model**: completare export e rasterizzazione PDFium con la stessa semantica osservabile
del backend Apple, senza introdurre ricostruzioni del PDF o conversioni pixel ridondanti.

**Canonical owner**:
- backing bytes P01: source canonica per save;
- PDFium: raster page content;
- `renderer/ayra_PdfSafetyLimits.h`: limiti raster condivisi.

**Save contract**:
- il renderer e' read-only, quindi save = copia byte-preserving dei backing bytes;
- nessun `FPDF_SaveAsCopy`: ricostruire un documento non modificato e' lavoro inutile e puo'
  cambiare struttura/metadata;
- snapshot bytes sotto lock, file I/O fuori dalla lock;
- failure non modifica la destinazione `MemoryBlock`; file usa `replaceWithData`.

**Render contract**:
- tutte le chiamate FPDF restano sotto la lock process-wide P01;
- `FPDF_GetPageWidthF/HeightF` forniscono la display size che incorpora la /Rotate pagina;
- `FPDF_RenderPageBitmap(... rotate=0 ...)` evita una seconda rotazione;
- page content only, senza `FPDF_ANNOT`, coerente con CoreGraphics;
- backdrop bianco;
- `juce::SoftwareImageType` ARGB con stride/size verificati;
- Windows/Linux: `FPDFBitmap_BGRA` coincide col layout native `PixelARGB`;
- Android: aggiungere `FPDF_REVERSE_BYTE_ORDER` per il layout RGBA native JUCE;
- stessi limiti M02: 16384 px/lato e 64 Mi pixel.

**Acceptance criteria P02**:
- `saveToFile`, `saveToMemory`, `renderPage` reali e senza `jassertfalse`;
- save byte-identico ai dati caricati;
- scale NaN/Inf/<=0 e raster over-budget -> failure deterministica;
- nessun post-process B/R su Win/Linux; Android usa flag PDFium;
- tutte le API FPDF di render sono serializzate;
- static diff audit completato;
- pixel/orientation/rotation + save equality su Win/Linux/Android: **external pending**.

---

## Esito P02

**P02 (2026-10-02)**: save file/memory e' byte-preserving dai backing bytes P01;
lo snapshot avviene sotto lock e l'I/O file fuori lock. Il raster usa
`juce::SoftwareImageType` + `FPDFBitmap_BGRA`, backdrop bianco, page content only,
limiti raster condivisi, e `rotate=0` perche' la page matrix PDFium incorpora la /Rotate.
Android usa `FPDF_REVERSE_BYTE_ORDER`; Win/Linux non eseguono post-process dei canali.
Handle page/bitmap vengono distrutti prima del rilascio della lock process-wide.
Pixel/orientation/save equality runtime: **external pending**.

## Change Contract — P03 PDFium text / search

**Task model**: completare il content model non-Apple con Unicode e geometria coerenti
con M04 Apple, senza parsing testuale proprietario.

**Canonical owner**:
- PDFium `fpdf_text.h`: estrazione, search e rettangoli;
- `PdfSearchResult`: semantica bounds cross-backend;
- `ayra_PdfSafetyLimits.h`: budget query/testo/risultati.

**Invariants**:
- tutte le API `FPDFText_*` usano la stessa lock process-wide P01;
- UTF-16LE e surrogate pair preservati; niente cast da wchar_t;
- `findText` case-insensitive (`flags=0`);
- exact match text estratto dal text page, non sostituito con la query;
- bounds = unione dei rect PDFium del range trovato, in page space lower-left/Y-up;
- risultati senza geometria valida non vengono pubblicati;
- max risultati superato -> failure atomico;
- char count/code units/query sono bounded prima di allocare;
- RAII chiude search handle, text page e page prima di rilasciare la lock.

**Acceptance criteria P03**:
- `extractText` e `findText` reali; nessun `jassertfalse` resta in PdfiumRenderer;
- Unicode BMP + surrogate pair preservati;
- match multiline produce bounds validi;
- pageIndex -1/targhettizzato validati;
- static diff audit completato;
- fixture Unicode/Type0/CMap/multiline + stress serializzazione: **external pending**.


**Esito P03 (2026-10-02)**: `extractText` usa `FPDFText_GetText` con buffer bounded
UTF-16LE e conversione code-unit per code-unit verso JUCE, senza aliasing tra
`FPDF_WCHAR` e `wchar_t/int16`. `findText` usa ricerca PDFium case-insensitive,
estrae il testo reale del match e calcola i bounds tramite `FPDFText_CountRects/GetRect`,
unendo i rect multiline in PDF page space. Overflow dei risultati = failure atomico.
Tutti i page/text/search handle hanno teardown RAII sotto la lock process-wide.
Nel renderer PDFium non restano `jassertfalse` o TODO.

**Esito D02**: `PdfDocument` delega ora a backend completi staticamente su Apple e
Win/Linux/Android. Questa e' completezza del codice, non certificazione runtime:
build/link/deployment e fixture cross-platform restano **external pending**.

---

## Change Contract — W01 PdfViewComponent core

**Task model**: rendere il viewer funzionale per caricamento, navigazione e rendering della
pagina corrente, mantenendo il widget come view del `PdfDocument` e una sola cache derivata.

**Canonical owner**:
- `PdfDocument`: documento, parsing, save e raster;
- `PdfPage`: bounds PDF + rotazione e display size derivata;
- `PdfViewComponent`: soltanto stato di presentazione (pagina 1-based, zoom/top-left) e cache;
- LookAndFeel: elementi decorativi del widget.

**Existing reusable path**:
- `PdfDocument::open/getPage/renderPage/save/saveToMemoryBlock`;
- `Component::BailOutChecker` + `ListenerList::callChecked` per callback lifetime-safe;
- default LookAndFeel del modulo.

**Expected files**:
- `engine/ayra_PdfPage.h` — helper trivial di display size;
- `renderer/mac/ayra_MacPdfRenderer.mm` — consuma l'helper, non duplica la formula;
- `widgets/ayra_PdfViewComponent.h/.cpp`;
- `widgets/look_and_feel/ayra_PdfDefaultLookAndFeel.h`;
- documentazione widget/stato.

**Forbidden ownership changes**:
- nessun parsing/search nel widget;
- nessuna copia autorevole del documento;
- nessuna gesture/pan/HiDPI/search overlay (W02);
- nessun alias o fallback legacy;
- nessuna ownership implicita del `PdfDocument&` esterno.

**Invariants**:
- `currentPage == 0` quando non esiste una pagina visualizzabile, altrimenti `1..pageCount`;
- conversione 1-based UI -> 0-based engine solo al boundary;
- display width/height scambiano gli assi solo per rotazioni 90/270, tramite owner `PdfPage`;
- cache immagine = rappresentazione derivata; invalidazione deterministica su documento/pagina;
- `paint()` non esegue parsing/raster: il raster gira su un worker dedicato del widget;
- publication worker -> Message Thread usa `SafePointer` + generation token, quindi risultati obsoleti vengono scartati;
- cambio/distruzione documento drena i job prima di rilasciare il lifetime non-owning;
- un load fallito lascia invariati documento corrente, pagina e cache;
- un documento esterno deve restare vivo e non essere mutato mentre e' agganciato;
- i Listener formali vengono notificati prima delle `std::function`, con bail-out se il callback distrugge il widget;
- nessun falso evento `closed` quando si cambia semplicemente documento;
- tutte le API widget e `paint` sono Message Thread only;
- W01 rasterizza off-thread a `currentZoom` e disegna l'immagine nei bounds logici; la device scale HiDPI appartiene a W02;
- i renderer concreti serializzano le proprie API: PDFium process-wide, Apple per istanza.

**Acceptance criteria W01**:
- file/memory load e `setDocument` attivano solo documenti aperti con almeno una pagina;
- stato/getter, navigazione 1-based, export e save-to-memory sono reali;
- `paint` fa solo compositing; nessuna chiamata `PdfDocument::renderPage` avviene sul Message Thread;
- rotazioni 90/270 producono bounds logici corretti;
- notifiche loaded/page-changed sono lifetime-safe;
- LookAndFeel ha forwarding default e un metodo dedicato allo sfondo pagina;
- nessun `jassertfalse`/TODO resta nelle responsabilita' W01;
- zoom/pan/gesture/HiDPI/search restano esplicitamente W02;
- build/component test GUI: **external pending**.

---

## Tabella riepilogativa

| File / Classe | Stato | Piattaforma | Fase |
|---|---|---|---|
| `engine/ayra_PdfPage.h` | ✅ completo | tutte | 1 |
| `engine/ayra_PdfSearchResult.h` | ✅ completo | tutte | 1 |
| `engine/ayra_PdfDocument.h` | ✅ interfaccia completa | tutte | 1 |
| `engine/ayra_PdfDocument.cpp` | ✅ factory/delega completa; Apple reale, PDFium dipende da P01-P03 | tutte | 4 |
| `renderer/ayra_PdfRenderer.h` | ✅ interfaccia completa | tutte | 1 |
| `renderer/mac/ayra_MacPdfRenderer.h` | ✅ dichiarazione completa | macOS/iOS | 1 |
| `renderer/mac/ayra_MacPdfRenderer.mm` | ✅ M01-M04 completi staticamente | macOS/iOS | 2 |
| `renderer/pdfium/ayra_PdfiumRenderer.h` | ✅ dichiarazione completa | Win/Linux/Android | 1 |
| `renderer/pdfium/ayra_PdfiumRenderer.cpp` | ✅ P01-P03 completi staticamente | Win/Linux/Android | 3 |
| `widgets/ayra_PdfViewComponent.h` | ✅ interfaccia completa | tutte | 1 |
| `widgets/ayra_PdfViewComponent.cpp` | 🟠 W01 in progress; W02 pending | tutte | 5 |
| `widgets/look_and_feel/ayra_PdfLookAndFeelMethods.h` | ✅ completo | tutte | 1 |
| `widgets/look_and_feel/ayra_PdfDefaultLookAndFeel.h/.cpp` | ✅ completo | tutte | 1 |
| `pdf_component/PDFComponent.h/.cpp` | ✅ funzionante (LEGACY) | macOS | - |
| `pdf_component/Mac_PDF_core/PDFView.m` | ✅ funzionante (LEGACY) | macOS | - |
| `pdf_component/Mac_PDF_core/MacPDFComponent.mm` | 🟠 funzionante con bug | macOS | - |
| `third_party/pdfium/include/` | ✅ header presenti (git) | Win/Linux | - |
| `third_party/pdfium/mac/libpdfium.dylib` | ✅ installata (chromium/7857) | macOS | - |
| `scripts/setup_pdfium.sh` | ✅ funzionante | macOS/Linux | - |
| `scripts/setup_pdfium.ps1` | ✅ funzionante | Windows | - |

---

## Cosa funziona oggi

**Solo il backend legacy macOS** tramite `PDFComponent` / `MacPDFViewComponent` / `PDFView`.

Funzionalita' operative:
- Apertura file PDF da path stringa (`loadDocument`)
- Apertura da buffer in memoria (`loadDocumentFromMemoryBlock`)
- Navigazione pagine (`setPageNumber`, `getTotPagesNum`, `getCurrentPageOnScreen`)
- Zoom con punto di handle (`setCurrentPageZoom`) — con il bug dell'anchor point
- Pan (`setCurrentPageTopLeftPosition`, `getCurrentPageTopLeftPosition`)
- Bounding box pagina corrente (`getCurrentPageBounds`)
- Export su file (`exportCurrentDocument`) — funzionante, usa `CGPDFContextCreateWithURL` con URL reale
- Dimensioni documento (`getDocumentWidth`, `getDocumentHeight`)

Funzionalita' non operative (mai implementate nel v1):
- `getMemoryBlockFromDocument` — rotto (vedi Bug 2)
- Ricerca testuale — mai implementata
- Estrazione testo — mai implementata
- Inspector metadati pagina (rotazione, tipo media box) — non esposti

---

## Bug noti — ordinati per priorita'

### Bug 1 — Shell injection in LinuxPDFViewComponent

**Severita'**: critica (vulnerabilita' di sicurezza)  
**File**: `pdf_component/Linux_PDF_core/LinuxPDFViewComponent.h` (stub)  
**Riga**: nella funzione `loadDocument` del componente Linux legacy  
**Descrizione**: il path del file veniva interpolato direttamente in una stringa passata a `system()`:

```cpp
// VULNERABILE
std::string cmd = "pdftocairo -png \"" + filePath + "\" /tmp/ayra_pdf_out";
system(cmd.c_str());
```

Se `filePath` contiene `;`, `&&`, `|`, `$()` o backtick, il codice aggiuntivo viene eseguito
con i permessi del processo host (DAW). Su Linux questo e' un rischio concreto se il plugin
e' usato con file PDF da fonti esterne.

**Soluzione**: il bug e' moot con il refactoring — `PdfiumRenderer` su Linux non usa subprocess.
Il file legacy non e' incluso nel build corrente.

---

### Bug 2 — getMemoryBlockFromDocument rotto

**Severita'**: alta (funzione restituisce dati corrotti silenziosamente)  
**File**: `pdf_component/Mac_PDF_core/MacPDFComponent.mm`  
**Riga**: 255-282  
**Descrizione**: la funzione usa `CGPDFContextCreateWithURL` con `@"dummyURL"`:

```objc
// v1 MacPDFComponent.mm:260 — rotto
CGContextRef pdfContext = CGPDFContextCreateWithURL(
    (__bridge CFURLRef)[NSURL URLWithString:@"dummyURL"], NULL, NULL);
```

`@"dummyURL"` non e' un file URL valido: `[NSURL URLWithString:@"dummyURL"]` ritorna un NSURL
con scheme `nil`, che non puo' essere usato come destinazione di scrittura. Il contesto viene
creato (non nil) ma i dati scritti nel "file" vengono silenziosamente ignorati da CoreGraphics.

Alla fine della funzione:
```objc
// v1 MacPDFComponent.mm:281 — produce junk
destData = juce::MemoryBlock { (void*)pdfData, sizeof(CGPDFDocumentRef) };
```

Questa riga copia i primi `sizeof(CGPDFDocumentRef)` byte (8 byte su 64-bit) del buffer
`pdfData` (che e' vuoto), producendo un `MemoryBlock` con 8 byte casuali dallo stack.

**Causa**: probabilmente copia di un pattern da un tutorial errato. Il pattern corretto
richiede `CGDataConsumerCreate` con un callback di scrittura verso `NSMutableData`.

**Soluzione**: vedi `03_fase2_MacPdfRenderer.md` sezione "Fix saveToMemory()".

---

### Bug 3 — Zoom anchor point formula errata

**Severita'**: media (UX degradata ma non crash)  
**File**: `pdf_component/Mac_PDF_core/MacPDFComponent.mm`  
**Riga**: 152-166 (`setCurrentPageZoom`)  
**Descrizione**:

```cpp
// v1 — formula errata
float xOffset = handlePoint.getX() - (handlePoint.getX() * pdfView.zoomLevel);
float yOffset = handlePoint.getY() - (handlePoint.getY() * pdfView.zoomLevel);
pdfView.topLeftOrigin = CGPointMake(origin.getX() + xOffset, origin.getY() + yOffset);
```

Il problema e' che `handlePoint` qui e' il mouse in viewport-space, ma `origin` e' il
`topLeftOrigin` del PDF anche (anch'esso in viewport-space). La formula calcola un offset
come `handle - handle * newZoom` che ignora completamente il vecchio zoom. L'ancora e' sempre
calcolata come se la pagina fosse posizionata all'origine (0,0), non alla posizione corrente.

**Effetto visibile**: durante lo zoom con pinch o scroll, la pagina scivola verso l'angolo
in alto a sinistra invece di rimanere ancorata al punto di contatto del mouse.

**Soluzione**: convertire l'anchor da viewport-space a PDF-space prima di applicare il nuovo zoom.
Vedi `03_fase2_MacPdfRenderer.md` sezione "Fix zoom anchor point".

---

### Bug 4 — Conteggio pagine approssimato su Windows/Linux legacy

**Severita'**: bassa (impatta solo piattaforme con stub)  
**File**: `pdf_component/Windows_PDF_core/` e `pdf_component/Linux_PDF_core/` (stub)  
**Descrizione**: il conteggio delle pagine avveniva cercando la stringa `/Page` nel buffer
grezzo del PDF senza parsing strutturale. I documenti con form fields, risorse XObject,
o strutture ad albero complesse producevano un conteggio errato (tipicamente superiore al reale).

**Soluzione**: il bug e' moot con il refactoring — `FPDF_GetPageCount()` conta le pagine
strutturalmente dal cross-reference table del PDF.

---

### Bug 5 — getCurrentPageBounds — origine CoreGraphics non convertita

**Severita'**: bassa (impatta documenti con MediaBox non in (0,0))  
**File**: `pdf_component/Mac_PDF_core/MacPDFComponent.mm`  
**Riga**: 177-183  
**Descrizione**: `CGPDFPageGetBoxRect` ritorna il rettangolo in PDF user space con origine
in basso a sinistra (Y-up). `juce::Rectangle` usa Y-down. La funzione copia i valori senza
conversione:

```cpp
// v1 — origine non convertita
return juce::Rectangle<float> {
    (float)mediaBox.origin.x,
    (float)mediaBox.origin.y,   // Y CoreGraphics != Y juce
    (float)mediaBox.size.width,
    (float)mediaBox.size.height
};
```

Per documenti standard con `MediaBox = [0 0 595 842]` questo non e' visibile.
Per documenti con `MediaBox = [36 72 595 842]` (margini non zero), le coordinate sono errate.

**Soluzione v2**: `MacPdfRenderer::getPage()` mantiene volutamente le bounds canoniche in PDF page space (72 dpi, lower-left/Y-up). La conversione verso coordinate JUCE/schermo appartiene esclusivamente al widget, evitando doppie conversioni.

---

## Dipendenze per piattaforma

| Piattaforma | Backend | Librerie richieste | File in third_party | Setup necessario |
|---|---|---|---|---|
| macOS | CoreGraphics | Framework Apple (automatico) | - | Nessuno |
| iOS | CoreGraphics | Framework Apple (automatico) | - | Nessuno |
| Windows | PDFium | pdfium.dll, pdfium.lib | `pdfium/win/` | `setup_pdfium.ps1` |
| Linux | PDFium | libpdfium.so | `pdfium/linux/` | `setup_pdfium.sh` |
| Android | PDFium | libpdfium.so (per ABI) | `pdfium/android/<abi>/` | `setup_pdfium.sh` |

### Note deployment

- **macOS**: `libpdfium.dylib` presente in `third_party/pdfium/mac/` (installata da `setup_pdfium.sh`).
  La dylib NON viene usata dal backend macOS (che usa CoreGraphics). E' disponibile per test futuri
  o per un eventuale utilizzo ibrido. Il backend macOS non dipende da PDFium.
- **Windows**: `pdfium.dll` deve essere distribuita accanto al `.vst3` o nel PATH di sistema.
  Il linker richiede `pdfium.lib` a compile-time, la DLL a runtime.
- **Linux**: `libpdfium.so` deve essere nella stessa directory del `.so` del plugin o in `LD_LIBRARY_PATH`.
  Usare `-Wl,-rpath,$$ORIGIN` per embedding nel pacchetto.
- **Android**: una `.so` per ogni ABI (arm64-v8a, armeabi-v7a, x86, x86_64) deve essere nel bundle APK.
