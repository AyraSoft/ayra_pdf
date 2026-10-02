include_guard(GLOBAL)

set(AYRA_PDF_TEST_SANITIZER "" CACHE STRING
    "Optional sanitizer for ayra_pdf contract runners: address, undefined, thread")
set_property(CACHE AYRA_PDF_TEST_SANITIZER PROPERTY STRINGS
    "" address undefined thread)

function(ayra_pdf_enable_test_sanitizer target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR
            "ayra_pdf_enable_test_sanitizer: unknown target '${target}'")
    endif()

    if(NOT AYRA_PDF_TEST_SANITIZER)
        return()
    endif()

    if(MSVC)
        message(FATAL_ERROR
            "AYRA_PDF_TEST_SANITIZER is currently supported only with GCC/Clang-style frontends")
    endif()

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        message(FATAL_ERROR
            "Unsupported compiler for AYRA_PDF_TEST_SANITIZER: ${CMAKE_CXX_COMPILER_ID}")
    endif()

    if(NOT AYRA_PDF_TEST_SANITIZER MATCHES "^(address|undefined|thread)$")
        message(FATAL_ERROR
            "Invalid AYRA_PDF_TEST_SANITIZER='${AYRA_PDF_TEST_SANITIZER}'")
    endif()

    target_compile_options("${target}" PRIVATE
        "-fsanitize=${AYRA_PDF_TEST_SANITIZER}"
        "-fno-omit-frame-pointer")

    target_link_options("${target}" PRIVATE
        "-fsanitize=${AYRA_PDF_TEST_SANITIZER}")
endfunction()
