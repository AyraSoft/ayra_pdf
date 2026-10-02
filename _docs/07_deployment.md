# Deployment — ayra_pdf

**Stato operativo**: vedere `_docs/02_stato_attuale.md`.

## Dependency source

I binari PDFium sono shared/dynamic e sono pin-nati in
`third_party/pdfium/pdfium_manifest.json`. Gli installer verificano SHA-256 prima
dell'estrazione.

## macOS / iOS

Backend CoreGraphics + PDFKit di sistema. Nessun binario PDFium nel bundle Apple.

## Windows

Provisioning:

```powershell
.\scripts\setup_pdfium.ps1 -Platform x64
```

File:
- link: `third_party/pdfium/win/x64/pdfium.dll.lib`;
- runtime: `third_party/pdfium/win/x64/pdfium.dll`.

La DLL deve essere accanto al **binario effettivo** del target.

```cmake
add_custom_command(TARGET MyPlugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Juce Modules/ayra_pdf/third_party/pdfium/win/x64/pdfium.dll"
        "$<TARGET_FILE_DIR:MyPlugin>/pdfium.dll")
```

## Linux

Provisioning:

```bash
./scripts/setup_pdfium.sh --platform linux --arch x64
```

Runtime: `third_party/pdfium/linux/x64/libpdfium.so`.

Se la `.so` viene copiata accanto al binario reale, usare RPATH `$ORIGIN`.

```cmake
set_target_properties(MyPlugin PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN")
```

## Android

Provisioning:

```bash
./scripts/setup_pdfium.sh --platform android --arch all
```

Output:
- `android/armeabi-v7a/libpdfium.so`;
- `android/arm64-v8a/libpdfium.so`;
- `android/x86/libpdfium.so`;
- `android/x86_64/libpdfium.so`.

Le `.so` devono essere incluse nel package sotto la ABI corrispondente e linkate
dal target nativo della stessa ABI.

## Release gate

- verificare runtime dependency loading;
- eseguire V01;
- non distribuire binari PDFium diversi dal manifest/header vendorizzati.
