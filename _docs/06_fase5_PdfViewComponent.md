# PdfViewComponent — Widget

**Stato operativo**: vedere `_docs/02_stato_attuale.md`.

## Contratto corrente

`PdfViewComponent` e' l'unico widget PDF pubblico Ayra.

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
- NaN/Inf rifiutati.

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

## Current-only

Il precedente widget `PDFComponent` e' stato rimosso. Nessun alias o adapter lo mantiene.

## Verification esterna

- navigazione;
- zoom anchor;
- pan/wheel/pinch;
- Retina/HiDPI e cambio display;
- pagine ruotate;
- query rapida e cambio pagina durante search;
- distruzione widget durante render/search;
- LookAndFeel custom.
