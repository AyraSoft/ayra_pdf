# PdfViewComponent — Widget

**Stato operativo**: vedere `_docs/02_stato_attuale.md`.

## Contratto corrente

`PdfViewComponent` e' l'unica implementazione canonica del widget PDF Ayra. Il nome pubblico `PDFComponent` e' un alias source-compatible dello stesso tipo.

Responsabilita':
- documento attivo tramite `std::shared_ptr<PdfDocument>`;
- navigazione utente 1-based;
- cache immagine derivata;
- zoom/pan/HiDPI;
- gesture;
- overlay search;
- LookAndFeel;
- Listener + callback inline.

Non possiede parsing, text engine o raster algorithm.

## Rendering asincrono

`paint()` fa soltanto compositing.

I metadata della pagina corrente vengono letti sincronicamente da `PdfDocument::getPage()`
quando cambia pagina e restano disponibili indipendentemente dal completamento del raster.
La cache asincrona riguarda soltanto l'immagine derivata. `setDocument(nullptr)` chiude il
documento visualizzato, invalida il lavoro asincrono pendente e riporta il widget allo stato vuoto.

Render e search usano un pool condiviso del modulo. Per ciascun widget:
- al massimo un job render schedulato;
- al massimo un job search schedulato;
- richieste successive vengono coalesced;
- latest-request-wins;
- generation atomica invalida risultati obsoleti;
- ogni job cattura uno `shared_ptr<PdfDocument>`.

Il raw owner nel `RenderState` viene letto/scritto soltanto sul Message Thread. `AsyncUpdater` cancella callback pending al teardown; il worker non accoda lambda non cancellabili nel MessageManager.

## Viewport

Zoom:
- range 0.1x..10x;
- anchor point stabile;
- NaN/Inf rifiutati;
- cambio pagina riuscito resetta a 1.0 per compatibilita' `PDFComponent`.

Pan:
- drag;
- wheel;
- setter programmatico;
- clamp con pagina centrata quando piu' piccola del viewport.

Pinch:
- `mouseMagnify`.

HiDPI:
- raster scale = logical zoom x device scale;
- dimensioni logiche pagina invarianti.

## Search overlay

`setSearchQuery()` cerca la pagina corrente via `PdfDocument::findText` sul worker.
Le bounds vengono convertite una sola volta da PDF page space a widget space, gestendo:
- MediaBox non-zero;
- Y-up -> Y-down;
- /Rotate 0/90/180/270.

L'highlight e' disegnato dal LookAndFeel.

## LookAndFeel

Hook:
- background;
- no-document;
- page background;
- page shadow;
- search highlight.

I default sono stateless e i metodi base fanno forwarding, cosi' l'app puo' override-are solo
cio' che serve.

## Eventi

Listener formali prima delle `std::function`.
Le notifiche sincrone usano `Component::BailOutChecker`.

Il contratto include:
- document loaded dopo una attivazione riuscita;
- document closed una sola volta quando `setDocument(nullptr)` chiude un documento attivo;
- page changed sulla navigazione;
- search results changed sulla publication degli highlight.

## Compatibilita' API

Il precedente percorso di implementazione `PDFComponent` e' stato rimosso. Il nome
`PDFComponent` resta come alias di `PdfViewComponent` per i consumer esistenti; non esiste
un adapter o un secondo widget.

Il contratto observable gia' usato dai consumer viene preservato anche senza documento:
`getTotPagesNum()`, `getCurrentPageOnScreen()` e `getCurrentPageZoom()` restituiscono `-1`;
la posizione top-left resta vuota e il relativo setter non modifica stato.

## Verification esterna

- navigazione;
- zoom anchor;
- pan/wheel/pinch;
- Retina/HiDPI e cambio display;
- pagine ruotate;
- query rapida e cambio pagina durante search;
- distruzione widget durante render/search;
- LookAndFeel custom.


## Legacy API compile guard

The module test suite statically checks the complete historical `PDFComponent` method surface
(load, navigation, geometry, zoom/pan, export and memory I/O), in addition to the type alias itself.
This makes source-compatibility regression a compile-time failure instead of a consumer-only discovery.


## Search resource guard

`PdfViewComponent::setSearchQuery()` enforces the same canonical UTF-8 query budget as the
renderers before retaining/copying the query into worker state. Oversized queries fail closed,
clear derived highlights, and never reach the worker/backend.


## Raster budget vs visual zoom

Visual zoom and raster resolution are intentionally separate. The page may remain at the requested
0.1x..10x visual zoom while the worker raster scale is capped to the canonical dimension/pixel
budgets. The cached image is then composited into the visual page bounds. This prevents valid high
zoom/HiDPI states from turning blank merely because a full-resolution raster would exceed the safety
budget.


## Allocation failure on direct load

Direct widget file/memory loads stage the replacement `PdfDocument` inside a `bad_alloc` boundary.
Allocation failure preserves the active document and all current view state, matching the same
transactional rule used for malformed/unsupported PDF input.


## Search invalidation publication

Changing a non-empty query now makes invalidation observable at the widget boundary: if highlights
were visible, the new worker request is staged first and listeners are then synchronously notified
with result count 0. Consumers no longer need to maintain a parallel “stale count” reset policy.


Page navigation also publishes search invalidation when the previous page had visible matches. The
ordering is page-changed first, then search-count=0, with a `BailOutChecker` between callbacks so a
listener may destroy the widget safely. Consumers therefore do not own page/search coherence.


## Document/search invalidation ownership

Document replacement and explicit close publish search-count invalidation from the widget when
visible highlights existed. Ordering is document loaded/closed first, then search-count=0, with a
`BailOutChecker` between callbacks. Consumers no longer own match-count reset policy for query,
page, document replacement, or document close.
