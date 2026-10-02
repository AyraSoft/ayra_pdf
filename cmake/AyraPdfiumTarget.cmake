include_guard(GLOBAL)

# Links the PDFium runtime selected by ayra_pdf's pinned manifest to an existing
# target. Apple is a no-op because ayra_pdf uses system CoreGraphics + PDFKit.
#
# Usage:
#   ayra_pdf_link_pdfium(MyTarget COPY_RUNTIME)
#
# COPY_RUNTIME copies the shared library next to the built executable/library
# on desktop targets and adds $ORIGIN RPATH on Linux. Product packaging may
# omit this option and own deployment explicitly.
function(ayra_pdf_link_pdfium target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "ayra_pdf_link_pdfium: unknown target '${target}'")
    endif()

    if(APPLE)
        return()
    endif()

    cmake_parse_arguments(AYRA_PDF "COPY_RUNTIME" "" "" ${ARGN})

    get_filename_component(
        AYRA_PDF_MODULE_ROOT
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.."
        ABSOLUTE)

    set(AYRA_PDF_MANIFEST
        "${AYRA_PDF_MODULE_ROOT}/third_party/pdfium/pdfium_manifest.json")

    if(NOT EXISTS "${AYRA_PDF_MANIFEST}")
        message(FATAL_ERROR "ayra_pdf PDFium manifest not found: ${AYRA_PDF_MANIFEST}")
    endif()

    file(READ "${AYRA_PDF_MANIFEST}" AYRA_PDF_MANIFEST_JSON)

    if(ANDROID)
        set(AYRA_PDF_PLATFORM "android")
        set(AYRA_PDF_ABI "${CMAKE_ANDROID_ARCH_ABI}")

        if(AYRA_PDF_ABI STREQUAL "armeabi-v7a")
            set(AYRA_PDF_ARCH "arm")
        elseif(AYRA_PDF_ABI STREQUAL "arm64-v8a")
            set(AYRA_PDF_ARCH "arm64")
        elseif(AYRA_PDF_ABI STREQUAL "x86")
            set(AYRA_PDF_ARCH "x86")
        elseif(AYRA_PDF_ABI STREQUAL "x86_64")
            set(AYRA_PDF_ARCH "x64")
        else()
            message(FATAL_ERROR
                "Unsupported Android ABI for PDFium: ${CMAKE_ANDROID_ARCH_ABI}")
        endif()

        set(AYRA_PDF_PROVISIONED_DIR
            "${AYRA_PDF_MODULE_ROOT}/third_party/pdfium/android/${AYRA_PDF_ABI}")
    else()
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" AYRA_PDF_PROCESSOR)

        if(AYRA_PDF_PROCESSOR MATCHES "^(x86_64|amd64)$")
            set(AYRA_PDF_ARCH "x64")
        elseif(AYRA_PDF_PROCESSOR MATCHES "^(i[3-6]86|x86)$")
            set(AYRA_PDF_ARCH "x86")
        elseif(AYRA_PDF_PROCESSOR MATCHES "^(aarch64|arm64)$")
            set(AYRA_PDF_ARCH "arm64")
        elseif(AYRA_PDF_PROCESSOR MATCHES "^arm")
            set(AYRA_PDF_ARCH "arm")
        else()
            message(FATAL_ERROR
                "Unsupported PDFium architecture: ${CMAKE_SYSTEM_PROCESSOR}")
        endif()

        if(WIN32)
            set(AYRA_PDF_PLATFORM "win")
        elseif(UNIX)
            set(AYRA_PDF_PLATFORM "linux")
        else()
            message(FATAL_ERROR
                "ayra_pdf PDFium target unsupported on this platform")
        endif()

        set(AYRA_PDF_PROVISIONED_DIR
            "${AYRA_PDF_MODULE_ROOT}/third_party/pdfium/"
            "${AYRA_PDF_PLATFORM}/${AYRA_PDF_ARCH}")
    endif()

    set(AYRA_PDF_ASSET_KEY "${AYRA_PDF_PLATFORM}-${AYRA_PDF_ARCH}")

    string(JSON AYRA_PDF_RUNTIME_ARCHIVE_PATH
        ERROR_VARIABLE AYRA_PDF_RUNTIME_JSON_ERROR
        GET "${AYRA_PDF_MANIFEST_JSON}"
        assets "${AYRA_PDF_ASSET_KEY}" runtime_path)

    if(AYRA_PDF_RUNTIME_JSON_ERROR)
        message(FATAL_ERROR
            "No PDFium runtime for ${AYRA_PDF_ASSET_KEY}: "
            "${AYRA_PDF_RUNTIME_JSON_ERROR}")
    endif()

    get_filename_component(
        AYRA_PDF_RUNTIME_NAME
        "${AYRA_PDF_RUNTIME_ARCHIVE_PATH}"
        NAME)

    set(AYRA_PDF_RUNTIME_FILE
        "${AYRA_PDF_PROVISIONED_DIR}/${AYRA_PDF_RUNTIME_NAME}")

    if(NOT EXISTS "${AYRA_PDF_RUNTIME_FILE}")
        message(FATAL_ERROR
            "PDFium runtime is not provisioned at ${AYRA_PDF_RUNTIME_FILE}. "
            "Run ayra_pdf/scripts/setup_pdfium for ${AYRA_PDF_ASSET_KEY}.")
    endif()

    if(WIN32)
        string(JSON AYRA_PDF_LINK_ARCHIVE_PATH
            ERROR_VARIABLE AYRA_PDF_LINK_JSON_ERROR
            GET "${AYRA_PDF_MANIFEST_JSON}"
            assets "${AYRA_PDF_ASSET_KEY}" link_path)

        if(AYRA_PDF_LINK_JSON_ERROR)
            message(FATAL_ERROR
                "No PDFium link_path for ${AYRA_PDF_ASSET_KEY}: "
                "${AYRA_PDF_LINK_JSON_ERROR}")
        endif()

        get_filename_component(
            AYRA_PDF_LINK_NAME
            "${AYRA_PDF_LINK_ARCHIVE_PATH}"
            NAME)

        set(AYRA_PDF_LINK_FILE
            "${AYRA_PDF_PROVISIONED_DIR}/${AYRA_PDF_LINK_NAME}")

        if(NOT EXISTS "${AYRA_PDF_LINK_FILE}")
            message(FATAL_ERROR
                "PDFium import library is not provisioned at ${AYRA_PDF_LINK_FILE}")
        endif()

        target_link_libraries("${target}" PRIVATE "${AYRA_PDF_LINK_FILE}")
    else()
        target_link_libraries("${target}" PRIVATE "${AYRA_PDF_RUNTIME_FILE}")
    endif()

    if(AYRA_PDF_COPY_RUNTIME AND NOT ANDROID)
        if(UNIX AND NOT APPLE)
            set_property(TARGET "${target}" APPEND PROPERTY BUILD_RPATH "$ORIGIN")
            set_property(TARGET "${target}" APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
        endif()

        add_custom_command(TARGET "${target}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${AYRA_PDF_RUNTIME_FILE}"
                "$<TARGET_FILE_DIR:${target}>/${AYRA_PDF_RUNTIME_NAME}")
    endif()
endfunction()
