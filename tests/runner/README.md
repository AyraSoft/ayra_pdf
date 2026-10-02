# ayra_pdf core contract runner

Standalone runner for the module-owned `PDF core contract` JUCE unit-test suite.

It supports two modes:

- default GUI-capable mode, including `PdfViewComponent` contracts;
- `AYRA_PDF_TEST_HEADLESS=ON`, excluding the widget and proving the headless dependency envelope.

Example:

```bash
cmake -S tests/runner -B build/pdf-core \
  -DAYRA_JUCE_DIR=/path/to/JUCE \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/pdf-core --config Release
```

Headless:

```bash
cmake -S tests/runner -B build/pdf-core-headless \
  -DAYRA_JUCE_DIR=/path/to/JUCE \
  -DAYRA_PDF_TEST_HEADLESS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/pdf-core-headless --config Release
```

On Windows/Linux/Android, PDFium linkage is resolved through
`cmake/AyraPdfiumTarget.cmake`, which reads the pinned module manifest. Desktop runners request
`COPY_RUNTIME`, so the shared runtime is copied beside the executable. Provision PDFium first with
the module setup scripts.

This repository workflow writes and audits the runner but does not execute it here.


On a native build, CTest registration is automatic:

```bash
ctest --test-dir build/pdf-core --output-on-failure -C Release
```

CTest registration is omitted while cross-compiling; deployment/device execution remains owned by
the platform-specific V01 step.


## Sanitizers

With a GCC/Clang-style frontend, configure one sanitizer per build directory:

```bash
cmake -S tests/runner -B build/pdf-core-asan \
  -DAYRA_JUCE_DIR=/path/to/JUCE \
  -DAYRA_PDF_TEST_SANITIZER=address \
  -DCMAKE_BUILD_TYPE=Debug
```

Supported values are `address`, `undefined`, and `thread`. Keep separate build directories; ASan
and TSan are not combined into one target. MSVC-specific sanitizer configuration remains an external
toolchain concern rather than being approximated with incompatible flags.


GUI-capable runner builds define `JUCE_MODAL_LOOPS_PERMITTED=1` only for the contract target. This
is required solely to drain `AsyncUpdater` publications deterministically in teardown/search tests;
headless builds do not enable modal loops.


The module test source itself remains compilable in any `JUCE_UNIT_TESTS=1` consumer: tests that
need `MessageManager::runDispatchLoopUntil()` are guarded by `JUCE_MODAL_LOOPS_PERMITTED`. The
canonical GUI runner enables that flag to execute the full async-publication coverage; other unit-test
hosts may omit it without compile failure.
