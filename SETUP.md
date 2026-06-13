# SETUP — ayra_pdf

Guida di configurazione per ogni piattaforma. Leggere anche `README.md` per l'architettura e la descrizione del modulo.

---

## Passo 0 — Aggiungere il modulo in Projucer (tutti i sistemi)

Questo passo e' necessario per **ogni** progetto che usa `ayra_pdf`, su qualsiasi piattaforma.

1. Aprire il file `.jucer` del progetto in Projucer.
2. Nel pannello laterale sinistro, selezionare **Modules**.
3. Cliccare il pulsante **+** in basso.
4. Scegliere **Add a module from a specified folder...**
5. Navigare fino alla cartella `Juce Modules/ayra_pdf/` e selezionarla.
6. Il modulo appare nella lista. Verificare che `juce_gui_extra` sia anch'esso presente (dipendenza automatica).

Fatto. Per **macOS e iOS** non serve nessun altro passo — fermarsi qui.
Per **Windows, Linux, Android** continuare con le sezioni seguenti (richiede PDFium, implementato nella Fase 3).

---

## macOS / iOS

Backend: **CoreGraphics** nativo (macOS 11+ / iOS 14+). Zero dipendenze esterne, zero configurazione aggiuntiva.

Dopo aver completato il Passo 0, il progetto compila e funziona. Non e' necessario scaricare PDFium ne' modificare Library Search Paths, Framework Search Paths o Extra Linker Flags.

### macOS / iOS - PDFium dylib deployment (solo se PDFium e' linkato anche su Mac)

Alcuni progetti dell'ecosistema Ayra linkano PDFium **anche** su macOS
(`externalLibraries="pdfium"` in `.jucer`, oppure `target_link_libraries` con
`libpdfium.dylib` su CMake). In quel caso il backend CoreGraphics resta vivo,
ma il linker risolve i simboli PDFium attraverso `libpdfium.dylib`.

**Nota PDFium su macOS:** bblanchon distribuisce **solo** `.dylib` per Mac (in
tutte le release pubblicate, sia recenti che storiche — la statica .a per Mac
non esiste come prebuild). Quindi servono tre cose, **tutte configurabili una
volta sola dentro il `.jucer`**, senza mai aprire Xcode a mano:

#### 1. install_name del dylib = `@rpath/libpdfium.dylib`  *(automatico)*

Le release di [bblanchon/pdfium-binaries](https://github.com/bblanchon/pdfium-binaries)
distribuiscono `libpdfium.dylib` con `install_name = ./libpdfium.dylib`. Quel
path relativo fa fallire `dyld` al runtime (la libreria viene cercata nel
**current working directory** del processo, non nella cartella del binario).

Dal 2026-05 lo script `scripts/setup_pdfium.sh` riscrive automaticamente
l'install_name a `@rpath/libpdfium.dylib` e ri-firma il dylib subito dopo
`lipo`. **Non serve fare nulla a mano**: basta eseguire (o ri-eseguire) lo
script una volta dopo il clone del repo. Verifica:

```bash
otool -D "Juce Modules/ayra_pdf/third_party/pdfium/mac/libpdfium.dylib"
# Output atteso: @rpath/libpdfium.dylib
```

#### 2. LC_RPATH del binario = `@loader_path`  *(campo Projucer)*

Nel `.jucer`, exporter Xcode (macOS), il campo **Extra Linker Flags** del
target deve contenere:

```
-Wl,-rpath,@loader_path
```

`@loader_path` punta alla cartella che contiene il binario in esecuzione —
per uno standalone e' `MyApp.app/Contents/MacOS/`, per un VST3 e'
`MyPlugin.vst3/Contents/MacOS/`. Combinato con install_name
`@rpath/libpdfium.dylib`, `dyld` cerca il dylib accanto al binario.

Si scrive UNA volta in Projucer e resta scritto.

#### 3. Copia del dylib nel bundle  *(campo `postbuildCommand` del `.jucer`)*

Il dylib NON viene copiato automaticamente da Projucer/Xcode. La prassi
canonica e' aggiungere lo snippet sotto nel campo **Post-build Script**
dell'exporter Xcode in Projucer (UI: Exporters -> Xcode (macOS) -> "Post-build
Shell Script"). Si scrive UNA volta — Projucer lo serializza nel `.jucer`
come attributo `postbuildCommand` e lo re-inietta a ogni "Save Project and
Open in IDE". **Mai piu' da toccare a mano dentro Xcode.**

```bash
# ayra_pdf: copia libpdfium.dylib accanto al binario nel bundle (.app/.vst3/.component)
SRC="$SRCROOT/../../../../Juce Modules/ayra_pdf/third_party/pdfium/mac/libpdfium.dylib"
if [ -f "$SRC" ]; then
  for KIND in app vst3 component; do
    DEST="$BUILT_PRODUCTS_DIR/$PRODUCT_NAME.$KIND/Contents/MacOS"
    if [ -d "$DEST" ]; then
      cp -f "$SRC" "$DEST/"
      codesign --force --sign - "$DEST/libpdfium.dylib" 2>/dev/null || true
    fi
  done
else
  echo "ayra_pdf: $SRC non trovato -- esegui Juce Modules/ayra_pdf/scripts/setup_pdfium.sh"
fi
```

> **Adattare la profondita' di `../`.** `$SRCROOT` punta a `Builds/MacOSX/`
> dell'exporter. Il numero di `../` per arrivare alla root del repo `Juce/`
> dipende da dove si trova il progetto:
> - progetto in `Juce/Juce Projects/MyPlugin/` -> `../../../../`
> - progetto in `Juce/MyPlugin/` -> `../../../`
> - in caso di dubbio: `cd $SRCROOT && pwd` mostra il path effettivo.

In alternativa, equivalente come attributo XML nel `.jucer` (Projucer scrive
esattamente questo quando salvi il campo nella GUI):

```xml
<XCODE_MAC ... externalLibraries="...pdfium"
           postbuildCommand="SRC=&quot;$SRCROOT/../../../../Juce Modules/ayra_pdf/third_party/pdfium/mac/libpdfium.dylib&quot;&#10;if [ -f &quot;$SRC&quot; ]; then&#10;  for KIND in app vst3 component; do&#10;    DEST=&quot;$BUILT_PRODUCTS_DIR/$PRODUCT_NAME.$KIND/Contents/MacOS&quot;&#10;    [ -d &quot;$DEST&quot; ] &amp;&amp; cp -f &quot;$SRC&quot; &quot;$DEST/&quot; &amp;&amp; codesign --force --sign - &quot;$DEST/libpdfium.dylib&quot; 2&gt;/dev/null&#10;  done&#10;fi">
```

**Vantaggio rispetto al Run Script Phase configurato a mano in Xcode:** se
Projucer rigenera il `.xcodeproj` (succede a ogni cambiamento di moduli /
flags / sources), una Build Phase aggiunta manualmente in Xcode viene
preservata da Xcode ma **non** dal flusso "Save and Open in IDE" di Projucer
quando rigenera da zero. Il `postbuildCommand` invece e' parte del `.jucer`
ed e' la fonte di verita': sopravvive a qualsiasi rigenerazione.

#### Riepilogo configurazione per-progetto (una volta sola)

| Dove | Campo | Valore |
|---|---|---|
| `.jucer` -> exporter Xcode | Extra Linker Flags | `-Wl,-rpath,@loader_path` |
| `.jucer` -> exporter Xcode | External Libraries to Link | `pdfium` (oltre alle altre) |
| `.jucer` -> exporter Xcode | Configurazione -> Library Search Paths | `<repo>/Juce Modules/ayra_pdf/third_party/pdfium/mac` |
| `.jucer` -> exporter Xcode | Post-build Shell Script | snippet sopra |
| `setup_pdfium.sh` (una volta dopo clone) | - | scarica + ri-firma `libpdfium.dylib` |

Da quel momento: ogni "Save and Open in IDE" + build da Xcode produce un
bundle gia' pronto, niente passi manuali, niente errori di `dyld`. Esempio
di progetto gia' configurato cosi': `Juce Projects/NodeGraph_Plugin/NodeGraph_Plugin.jucer`.

#### Fix di emergenza per un binario gia' compilato senza la configurazione sopra

Se ti capita un `.app` legacy (build di prima del `postbuildCommand`) che
crasha con `dyld: Library not loaded: ./libpdfium.dylib`, il fix manuale e':

```bash
APP="MyApp.app"
BIN="$APP/Contents/MacOS/MyApp"
install_name_tool -change ./libpdfium.dylib @rpath/libpdfium.dylib "$BIN"
cp "Juce Modules/ayra_pdf/third_party/pdfium/mac/libpdfium.dylib" "$APP/Contents/MacOS/"
codesign --force --sign - "$APP/Contents/MacOS/libpdfium.dylib"
codesign --force --sign - "$APP"
```

E poi aggiungi il `postbuildCommand` al `.jucer` perche' non capiti mai piu'.

---

## Windows

### Prerequisiti

- Windows 10 64-bit o superiore
- PowerShell 5.1 o superiore (incluso in Windows 10)
- Accesso a internet per il download dei binari PDFium

### 1. Scaricare i binari PDFium

Aprire PowerShell come amministratore nella root del repository Ayra ed eseguire:

```powershell
cd "Juce Modules/ayra_pdf"
.\scripts\setup_pdfium.ps1
```

Lo script scarica automaticamente:
- `third_party/pdfium/win/pdfium.dll`
- `third_party/pdfium/win/pdfium.lib`

Al termine lo script stampa la versione installata e la checksum verificata.

### 2. Configurazione Projucer

Aprire il file `.jucer` del progetto in Projucer. Per l'exporter **Visual Studio (Windows)**:

| Campo | Valore |
|-------|--------|
| Extra Library Search Paths | `$(module_path)/third_party/pdfium/win` |
| Extra Linker Flags | `pdfium.lib` |

dove `$(module_path)` e' il percorso assoluto della cartella `Juce Modules/ayra_pdf`.

### 3. Distribuire pdfium.dll

`pdfium.dll` deve essere nella stessa cartella del file `.vst3` / `.exe` al momento dell'esecuzione. Aggiungerla al post-build step o al pacchetto di distribuzione.

---

## Linux

### Prerequisiti

- Ubuntu 20.04+ / Debian 11+ / Fedora 35+ (o equivalente)
- `curl` installato (`sudo apt install curl` / `sudo dnf install curl`)
- `tar` (incluso di default)

### 1. Scaricare i binari PDFium

Dalla root del repository:

```bash
cd "Juce Modules/ayra_pdf"
chmod +x scripts/setup_pdfium.sh
./scripts/setup_pdfium.sh
```

Lo script scarica e installa:
- `third_party/pdfium/linux/libpdfium.a` (static, architettura corrente)

> **Nota:** le release recenti di bblanchon/pdfium-binaries (>= chromium/7000) distribuiscono
> `.a` statica su Linux. Su macOS distribuiscono `.dylib`. Lo script gestisce entrambi automaticamente.

Al termine stampa la versione installata.

### 2. Configurazione Projucer

Per l'exporter **Linux Makefile** o **Code::Blocks** in Projucer:

| Campo | Valore |
|-------|--------|
| Extra Library Search Paths | `$(module_path)/third_party/pdfium/linux` |
| Extra Linker Flags | `-lpdfium -Wl,-rpath,\$ORIGIN` |

Il flag `-Wl,-rpath,\$ORIGIN` e' necessario se si usa la versione condivisa. Con la static library puo' essere omesso.

---

## Android

### Prerequisiti

- Android NDK r25c o superiore
- CMake 3.22 o superiore (incluso in Android Studio)
- ABIs target: `arm64-v8a` e/o `x86_64`

### 1. Scaricare i binari PDFium

Lo script `setup_pdfium.sh` con il flag `--android` scarica i `.so` per ogni ABI:

```bash
cd "Juce Modules/ayra_pdf"
./scripts/setup_pdfium.sh --android
```

Struttura risultante:
```
third_party/pdfium/android/
├── arm64-v8a/libpdfium.so
├── armeabi-v7a/libpdfium.so
└── x86_64/libpdfium.so
```

### 2. Configurazione CMakeLists.txt

Aggiungere nel `CMakeLists.txt` del progetto JUCE:

```cmake
if (ANDROID)
    set (AYRA_PDF_MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Juce Modules/ayra_pdf")

    target_link_libraries (MyPlugin PRIVATE
        "${AYRA_PDF_MODULE_DIR}/third_party/pdfium/android/${ANDROID_ABI}/libpdfium.so")

    # Copiare la .so nell'APK
    set_target_properties (MyPlugin PROPERTIES
        ANDROID_EXTRA_NATIVE_LIBS
        "${AYRA_PDF_MODULE_DIR}/third_party/pdfium/android/${ANDROID_ABI}/libpdfium.so")
endif()
```

---

## Projucer — Configurazione Dettagliata

### Aggiungere il modulo

Vedi **Passo 0** all'inizio di questo documento.

### Campi Projucer per piattaforma (Fase 3 — PDFium)

I campi seguenti sono necessari **solo dopo l'implementazione della Fase 3** (PdfiumRenderer).
Su macOS/iOS nessun campo aggiuntivo e' richiesto in nessuna fase.

#### macOS (Xcode exporter) — stato attuale e futuro

| Campo | Valore |
|-------|--------|
| Extra Frameworks | (nessuno — CoreGraphics e' automatico) |
| Extra Library Search Paths | (vuoto — CoreGraphics non richiede path aggiuntivi) |
| Extra Linker Flags | (vuoto) |

> **Nota dylib (Fase 3 opzionale):** se in futuro si abilita PDFium anche su macOS
> (non previsto — CoreGraphics resta il backend), aggiungere:
> - Extra Library Search Paths: `path/to/ayra_pdf/third_party/pdfium/mac`
> - Extra Linker Flags: `-lpdfium -Wl,-rpath,@loader_path`
> - Post-build script: copia `libpdfium.dylib` dentro `MyPlugin.vst3/Contents/MacOS/`

#### iOS (Xcode exporter) — stato attuale e futuro

Identico a macOS. Nessun campo aggiuntivo.

#### Windows (Visual Studio exporter) — Fase 3

| Campo | Valore |
|-------|--------|
| Extra Library Search Paths | `path\to\ayra_pdf\third_party\pdfium\win` |
| External Libraries to Link | `pdfium` |

Nota: usare backslash nei percorsi Windows in Projucer. Aggiungere `pdfium.dll` nella stessa cartella del plugin (post-build step o distribuzione manuale).

#### Linux (Linux Makefile exporter) — Fase 3

| Campo | Valore |
|-------|--------|
| Extra Library Search Paths | `/path/to/ayra_pdf/third_party/pdfium/linux` |
| External Libraries to Link | `pdfium` |
| Extra Linker Flags | `-Wl,-rpath,$$ORIGIN` |

---

## CMake — Configurazione Completa

```cmake
cmake_minimum_required (VERSION 3.22)

# Percorso del modulo (adattare)
set (AYRA_PDF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Juce Modules/ayra_pdf")

# Aggiungere il modulo JUCE
juce_add_module ("${AYRA_PDF_DIR}")

# Linkare il target
target_link_libraries (MyPlugin
    PRIVATE
        ayra_pdf
        juce::juce_gui_extra
)

# --- PDFium per piattaforme non-Apple ---
if (WIN32)
    target_link_libraries (MyPlugin PRIVATE
        "${AYRA_PDF_DIR}/third_party/pdfium/win/pdfium.lib")

    # Copia automatica della DLL nella directory di output
    add_custom_command (TARGET MyPlugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AYRA_PDF_DIR}/third_party/pdfium/win/pdfium.dll"
            "$<TARGET_FILE_DIR:MyPlugin>"
        COMMENT "Copia pdfium.dll nella directory di output")

elseif (UNIX AND NOT APPLE AND NOT ANDROID)
    target_link_libraries (MyPlugin PRIVATE
        "${AYRA_PDF_DIR}/third_party/pdfium/linux/libpdfium.a")
    target_link_options (MyPlugin PRIVATE "-Wl,-rpath,\$ORIGIN")

elseif (ANDROID)
    target_link_libraries (MyPlugin PRIVATE
        "${AYRA_PDF_DIR}/third_party/pdfium/android/${ANDROID_ABI}/libpdfium.so")
endif()

# --- Headless mode (opzionale) ---
# target_compile_definitions (MyPlugin PRIVATE AYRA_PDF_HEADLESS)
```

---

## Verifica Installazione

### macOS / iOS

Compilare il progetto. Se la build ha successo e il widget mostra il PDF senza crash, il setup e' corretto. Non ci sono check aggiuntivi (CoreGraphics non richiede file esterni).

### Windows

Dopo la build, verificare:

1. `pdfium.dll` e' presente nella stessa cartella del plugin/eseguibile.
2. Il plugin si carica in un host VST3 senza errori di "DLL not found".
3. Aprire un PDF dal plugin: la pagina deve essere renderizzata correttamente (non il placeholder).

Comando di verifica rapida (PowerShell):

```powershell
# Verifica che la DLL sia presente
Test-Path "Juce Modules/ayra_pdf/third_party/pdfium/win/pdfium.dll"

# Verifica la versione linkando un piccolo test
# (richiede che setup_pdfium.ps1 sia stato eseguito)
```

### Linux

```bash
# Verifica che la libreria static sia presente
ls -lh "Juce Modules/ayra_pdf/third_party/pdfium/linux/libpdfium.a"

# Verifica che i simboli PDFium siano nel binario compilato
nm -D MyPlugin.vst3/Contents/x86_64-linux/MyPlugin.so | grep FPDF_InitLibrary
```

### Android

```bash
# Verifica presenza .so per ABI
ls "Juce Modules/ayra_pdf/third_party/pdfium/android/arm64-v8a/libpdfium.so"

# Dopo il build, verificare che la .so sia nell'APK
unzip -l MyPlugin.apk | grep libpdfium
```

---

## Aggiornare PDFium

Per aggiornare alla versione piu' recente di PDFium:

### macOS / Linux

```bash
# Modificare la versione in cima allo script
nano "Juce Modules/ayra_pdf/scripts/setup_pdfium.sh"
# Cambiare: PDFIUM_VERSION="6xxx"

# Rieseguire lo script
./scripts/setup_pdfium.sh
```

### Windows

```powershell
# Modificare la versione in cima allo script
notepad "Juce Modules/ayra_pdf/scripts/setup_pdfium.ps1"
# Cambiare: $PDFIUM_VERSION = "6xxx"

# Rieseguire lo script
.\scripts\setup_pdfium.ps1
```

Le release disponibili si trovano su: https://github.com/bblanchon/pdfium-binaries/releases

Dopo l'aggiornamento, ricompilare il progetto intero per evitare mismatch tra header (in git) e binari.

---

## Troubleshooting

### `dyld: Library not loaded: ./libpdfium.dylib` (macOS)

**Sintomo completo:**
```
dyld[xxxxx]: Library not loaded: ./libpdfium.dylib
  Referenced from: <...> MyApp.app/Contents/MacOS/MyApp
  Reason: tried: 'MyApp.app/Contents/MacOS/libpdfium.dylib' (no such file),
         '/usr/lib/system/introspection/libpdfium.dylib' (no such file, not in dyld cache),
         './libpdfium.dylib' (no such file), ...
```

**Causa:** combinazione di due problemi:
1. Il `libpdfium.dylib` scaricato da bblanchon ha `install_name = ./libpdfium.dylib`
   (path relativo invalido — dyld interpreta `./` come **current working directory**).
2. Il `.dylib` non e' stato copiato nel bundle accanto al binario.

**Soluzione (fix permanente, 3 modifiche nel `.jucer` + setup_pdfium.sh):**

Tutta la configurazione vive nel `.jucer` (campi Projucer). Una volta scritta
non si tocca piu'. Vedi la sezione **macOS / iOS - PDFium dylib deployment**
sopra per il dettaglio completo dei tre campi:

1. **Setup binari**: ri-esegui `scripts/setup_pdfium.sh` -- dal 2026-05 lo
   script riscrive automaticamente l'install_name del dylib a
   `@rpath/libpdfium.dylib` e ri-firma. Una sola esecuzione dopo il clone
   del repo.

2. **.jucer -> Extra Linker Flags**: aggiungi `-Wl,-rpath,@loader_path`
   (verifica con `otool -l MyApp | grep -A2 LC_RPATH`).

3. **.jucer -> Post-build Shell Script**: incolla lo snippet che copia il
   dylib in `.app/.vst3/.component`. Da quel momento ogni "Save and Open in
   IDE" rigenera il `.xcodeproj` con la copia gia' configurata. Niente
   passi manuali in Xcode.

**Patch a caldo di un binario gia' compilato** (es. errore in produzione,
non vuoi ricompilare):

```bash
APP="MyApp.app"
BIN="$APP/Contents/MacOS/MyApp"

# 1. Riscrivi il riferimento nel binario
install_name_tool -change ./libpdfium.dylib @rpath/libpdfium.dylib "$BIN"

# 2. Copia il dylib (gia' con install_name @rpath) accanto al binario
cp "Juce Modules/ayra_pdf/third_party/pdfium/mac/libpdfium.dylib" \
   "$APP/Contents/MacOS/"

# 3. Ri-firma tutto
codesign --force --sign - "$APP/Contents/MacOS/libpdfium.dylib"
codesign --force --sign - "$APP"

# 4. Verifica
otool -L "$BIN" | grep pdfium    # deve stampare @rpath/libpdfium.dylib
```

### "pdfium.dll not found" (Windows)

**Causa:** la DLL non e' nella stessa cartella del plugin o in una directory nel PATH.

**Soluzione:**
1. Verificare che `setup_pdfium.ps1` sia stato eseguito: il file `third_party/pdfium/win/pdfium.dll` deve esistere.
2. Aggiungere il post-build step CMake per copiare automaticamente la DLL (vedi sezione CMake).
3. In alternativa, copiare manualmente `pdfium.dll` nella cartella `<progetto>/Builds/VisualStudio2022/x64/Release/` prima di eseguire.

### "cannot find -lpdfium" (Linux)

**Causa:** il percorso della libreria non e' configurato correttamente in Projucer / CMake.

**Soluzione:**
1. Verificare che `libpdfium.a` esista: `ls Juce\ Modules/ayra_pdf/third_party/pdfium/linux/`
2. Se manca, eseguire `./scripts/setup_pdfium.sh`.
3. Assicurarsi che il percorso in "Extra Library Search Paths" sia **assoluto**, non relativo.

### Crash in `FPDF_InitLibrary` (tutte le piattaforme)

**Causa:** `FPDF_InitLibrary()` chiamato piu' volte senza `FPDF_DestroyLibrary()` tra le chiamate, oppure chiamato da thread diversi contemporaneamente.

**Soluzione:** non chiamare `FPDF_InitLibrary()` direttamente — e' gestito internamente da `PdfiumRenderer` tramite `std::call_once`. Verificare che non ci siano chiamate manuali nel codice dell'applicazione.

### macOS: il PDF si carica ma non si vede (schermo bianco)

**Causa:** problema con il `NSViewComponent` / `UIViewComponent` che non aggiorna la view.

**Workaround (v1):** chiamare `repaint()` sul `PDFComponent` dopo `loadDocument()`. Il bug e' noto e sara' risolto nella Fase 4 con il nuovo `PdfViewComponent`.

### Android: UnsatisfiedLinkError per libpdfium.so

**Causa:** il file `.so` per l'ABI corretto non e' stato incluso nell'APK.

**Soluzione:**
1. Verificare che la `.so` per tutti gli ABI target sia presente in `third_party/pdfium/android/`.
2. Rieseguire `setup_pdfium.sh --android`.
3. Nel `CMakeLists.txt`, assicurarsi che `ANDROID_EXTRA_NATIVE_LIBS` includa la `.so`.

### Headers PDFium non trovati (compile error `fpdf_view.h not found`)

**Causa:** gli headers in `third_party/pdfium/include/` non sono nel include path del compilatore.

**Soluzione:** il modulo dovrebbe aggiungere automaticamente il percorso tramite la dichiarazione JUCE del modulo. Se non accade, aggiungere manualmente:

```
Extra Header Search Paths (Projucer):
    $(module_path)/third_party/pdfium/include
```

oppure in CMake:

```cmake
target_include_directories (MyPlugin PRIVATE
    "${AYRA_PDF_DIR}/third_party/pdfium/include")
```

---

## Struttura File Terze Parti — Cosa va in git

```
ayra_pdf/
└── third_party/
    └── pdfium/
        ├── include/        <- IN GIT: headers C dell'API pubblica PDFium
        │   ├── fpdf_view.h
        │   ├── fpdf_doc.h
        │   ├── fpdf_text.h
        │   ├── fpdf_save.h
        │   └── fpdfview.h
        ├── mac/            <- NON in git: libpdfium.dylib fat universal (arm64+x86_64)
        ├── win/            <- NON in git: pdfium.dll + pdfium.lib
        ├── linux/          <- NON in git: libpdfium.a x86_64 (statica)
        ├── ios/            <- NON in git: libpdfium.a fat (arm64+sim)
        └── android/        <- NON in git: libpdfium.so per ABI
```

### .gitignore (sezione rilevante)

Il `.gitignore` del modulo include gia' `*.a`, `*.lib`, `*.dll`, `*.so` a livello globale. Per documentare esplicitamente cosa non va in tracking, aggiungere nel `.gitignore` del repository padre:

```gitignore
# PDFium binaries (scaricati da setup_pdfium.sh/.ps1)
Juce Modules/ayra_pdf/third_party/pdfium/mac/
Juce Modules/ayra_pdf/third_party/pdfium/win/*.dll
Juce Modules/ayra_pdf/third_party/pdfium/win/*.lib
Juce Modules/ayra_pdf/third_party/pdfium/linux/
Juce Modules/ayra_pdf/third_party/pdfium/ios/
Juce Modules/ayra_pdf/third_party/pdfium/android/

# Gli headers PDFium sono in git e non vanno ignorati
!Juce Modules/ayra_pdf/third_party/pdfium/include/
```

**Motivazione per non committare i binari:**
1. Dimensione: 15-20 MB per piattaforma moltiplicati per le versioni aggiornat saturerebbe la history git.
2. Riproducibilita': `setup_pdfium.sh` garantisce lo stesso binario per ogni sviluppatore partendo dalla stessa `PDFIUM_VERSION`.
3. Licenza: i binari Apache 2.0 sono redistribuibili, ma e' preferibile che ogni sviluppatore li scarichi dal sorgente ufficiale per garantire l'autenticita'.
