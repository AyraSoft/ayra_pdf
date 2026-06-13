# Deployment — ayra_pdf

**Autore**: Ayra Soft  
**Data creazione**: 2026-05-26

---

## macOS — dylib nel bundle VST3

Il backend macOS usa CoreGraphics nativo e **non richiede** `libpdfium.dylib` a runtime.
La dylib e' presente in `third_party/pdfium/mac/` ma il build di produzione macOS non la include.

### Se in futuro si usa PDFium anche su macOS (opzionale)

La dylib (`libpdfium.dylib`, versione chromium/7857, fat binary arm64+x86_64) deve essere
copiata nel bundle del plugin nel post-build step.

Struttura bundle VST3 richiesta:

```
MyPlugin.vst3/
└── Contents/
    ├── Info.plist
    ├── MacOS/
    │   ├── MyPlugin           <- binario del plugin
    │   └── libpdfium.dylib    <- dylib PDFium (stesso livello del binario)
    └── Resources/
```

### Flag linker obbligatori

Nel Projucer (Extra Linker Flags) o in CMake:

```
-Wl,-rpath,@loader_path
```

Questo dice al dynamic linker di cercare le dylib nella stessa directory del binario
(`@loader_path` = directory di `MyPlugin`). Senza questo flag, il linker cerca la dylib
nei path di sistema e il plugin non si carica.

Verifica al termine del build:
```bash
otool -L MyPlugin.vst3/Contents/MacOS/MyPlugin | grep pdfium
# Deve mostrare: @rpath/libpdfium.dylib
```

### Post-build step Xcode

In Xcode -> Build Phases -> + New Run Script Phase:

```bash
# Copia libpdfium.dylib nel bundle dopo la build
PDFIUM_SRC="${PROJECT_DIR}/Juce Modules/ayra_pdf/third_party/pdfium/mac/libpdfium.dylib"
BUNDLE_MACOS="${CODESIGNING_FOLDER_PATH}/Contents/MacOS"
cp "$PDFIUM_SRC" "$BUNDLE_MACOS/libpdfium.dylib"
```

### Post-build step CMake

```cmake
add_custom_command(TARGET MyPlugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/Juce Modules/ayra_pdf/third_party/pdfium/mac/libpdfium.dylib"
        "$<TARGET_BUNDLE_CONTENT_DIR:MyPlugin>/MacOS/libpdfium.dylib"
    COMMENT "Copia libpdfium.dylib nel bundle VST3"
)
```

### Code signing con dylib

Se il bundle viene firmato con `codesign`, la dylib deve essere firmata per prima:

```bash
codesign --force --verify --timestamp \
    --sign "Developer ID Application: Ayra Soft" \
    MyPlugin.vst3/Contents/MacOS/libpdfium.dylib

codesign --force --verify --timestamp \
    --sign "Developer ID Application: Ayra Soft" \
    MyPlugin.vst3
```

---

## Windows — DLL accanto al VST3

`pdfium.dll` deve essere disponibile a runtime nella stessa cartella del `.vst3`
o in una directory nel PATH di sistema.

Struttura consigliata:

```
C:\Program Files\VSTPlugins\
├── MyPlugin.vst3
└── pdfium.dll          <- stesso livello del VST3
```

### Post-build step CMake Windows

```cmake
if(WIN32)
    add_custom_command(TARGET MyPlugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/Juce Modules/ayra_pdf/third_party/pdfium/win/pdfium.dll"
            "$<TARGET_FILE_DIR:MyPlugin>/pdfium.dll"
        COMMENT "Copia pdfium.dll nella directory del plugin"
    )
endif()
```

### Installer NSIS / WiX

L'installer deve includere `pdfium.dll` come componente e installarlo nella stessa directory
del plugin. Aggiungere al manifesto NSIS:

```nsis
File "third_party\pdfium\win\pdfium.dll"
```

---

## Linux — RPATH

Su Linux, il plugin `.so` deve trovare `libpdfium.so` a runtime. La soluzione piu' robusta
e' specificare `$ORIGIN` come RPATH, che dice al dynamic linker di cercare nella stessa
directory del file caricante.

### CMake

```cmake
if(UNIX AND NOT APPLE)
    set_target_properties(MyPlugin PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
    # Flag aggiuntivo per assicurarsi che il linker accetti $ORIGIN
    target_link_options(MyPlugin PRIVATE "-Wl,-rpath,$ORIGIN")
endif()
```

### Projucer — Extra Linker Flags

```
-Wl,-rpath,$ORIGIN
```

Nota: in alcune shell `$` deve essere escapato come `$$ORIGIN` in Makefile, ma non in CMake.

### Distribuzione

Il pacchetto di installazione deve includere `libpdfium.so` nella stessa directory del
file `.so` del plugin. Esempio con tar.gz:

```
MyPlugin/
├── MyPlugin.so
└── libpdfium.so
```

---

## iOS — static lib nel bundle

Su iOS il dynamic linking di librerie non di sistema non e' consentito dallo store.
`libpdfium.a` (static archive) viene linkato direttamente nel binario del plugin.

Non sono necessari step di deployment extra: la libreria e' compilata dentro il `.app`.

Setup CMake:

```cmake
if(IOS)
    target_link_libraries(MyPlugin PRIVATE
        "${CMAKE_SOURCE_DIR}/Juce Modules/ayra_pdf/third_party/pdfium/ios/libpdfium.a"
    )
endif()
```

L'archivio iOS di PDFium deve essere una fat library (o xcframework) per supportare
sia Simulator (x86_64 / arm64) che Device (arm64). Lo script `setup_pdfium.sh` scarica
gia' la versione con architecture corretta se `PLATFORM=ios`.

---

## Android — .so nell'APK

PDFium per Android richiede un `.so` per ogni ABI supportata. Le ABI tipiche:
- `arm64-v8a` — dispositivi ARM64 moderni (priorita' 1)
- `armeabi-v7a` — dispositivi ARM32 legacy
- `x86_64` — emulatori AVD
- `x86` — emulatori AVD legacy

### Struttura in third_party

```
third_party/pdfium/android/
├── arm64-v8a/libpdfium.so
├── armeabi-v7a/libpdfium.so
├── x86_64/libpdfium.so
└── x86/libpdfium.so
```

### CMake Android

```cmake
if(ANDROID)
    target_link_libraries(MyPlugin PRIVATE
        "${CMAKE_SOURCE_DIR}/Juce Modules/ayra_pdf/third_party/pdfium/android/${ANDROID_ABI}/libpdfium.so"
    )
    # Includi la .so nell'APK per l'ABI corrente
    set_property(TARGET MyPlugin APPEND PROPERTY
        ANDROID_EXTRA_NATIVE_LIBS
        "${CMAKE_SOURCE_DIR}/Juce Modules/ayra_pdf/third_party/pdfium/android/${ANDROID_ABI}/libpdfium.so"
    )
endif()
```

La variabile `ANDROID_ABI` e' impostata da CMake Android toolchain (`arm64-v8a`, ecc.).
Android Gradle plugin copiera' automaticamente i `.so` di tutte le ABI nelle posizioni corrette.

---

## Riepilogo per piattaforma

| Piattaforma | Formato | Deploy | Note |
|---|---|---|---|
| macOS CoreGraphics | n/a | Nessuno | Backend nativo, zero dipendenze esterne |
| macOS PDFium (futuro) | .dylib | `Contents/MacOS/` + RPATH `@loader_path` | Firma separata prima del bundle |
| Windows | .dll | Stessa cartella del .vst3 | In PATH come alternativa |
| Linux | .so | Stessa cartella del .so | RPATH `$ORIGIN` nel linker |
| iOS | .a | Linkato staticamente | Fat binary Sim+Device consigliato |
| Android | .so | APK, una per ABI | 4 ABI = 4 .so separati |
