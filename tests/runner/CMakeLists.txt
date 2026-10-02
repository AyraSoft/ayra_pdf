cmake_minimum_required(VERSION 3.22)

project(AyraPdfCoreContractTests VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(AYRA_PDF_TEST_HEADLESS
    "Build ayra_pdf contract tests without PdfViewComponent/juce_gui_basics"
    OFF)

set(AYRA_JUCE_DIR "" CACHE PATH "Path to the JUCE source checkout")

if(NOT AYRA_JUCE_DIR)
    if(DEFINED ENV{JUCE_DIR} AND EXISTS "$ENV{JUCE_DIR}/CMakeLists.txt")
        set(AYRA_JUCE_DIR "$ENV{JUCE_DIR}")
    elseif(APPLE AND EXISTS "/Applications/JUCE/CMakeLists.txt")
        set(AYRA_JUCE_DIR "/Applications/JUCE")
    else()
        message(FATAL_ERROR
            "JUCE not found. Pass -DAYRA_JUCE_DIR=/path/to/JUCE or set JUCE_DIR.")
    endif()
endif()

add_subdirectory("${AYRA_JUCE_DIR}" JUCE)
get_filename_component(AYRA_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

juce_add_module("${AYRA_MODULES}/ayra_pdf")

juce_add_console_app(AyraPdfCoreContractTests
    PRODUCT_NAME "Ayra PDF Core Contract Tests"
    COMPANY_NAME "Ayra")

target_sources(AyraPdfCoreContractTests PRIVATE Main.cpp)

target_compile_definitions(AyraPdfCoreContractTests PRIVATE
    JUCE_UNIT_TESTS=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:AyraPdfCoreContractTests,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:AyraPdfCoreContractTests,JUCE_VERSION>")

if(AYRA_PDF_TEST_HEADLESS)
    target_compile_definitions(AyraPdfCoreContractTests PRIVATE
        AYRA_PDF_HEADLESS=1)
else()
    target_link_libraries(AyraPdfCoreContractTests PRIVATE
        juce::juce_gui_basics)
endif()

if(MSVC)
    target_compile_options(AyraPdfCoreContractTests PRIVATE
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Release>:/O2>)
else()
    target_compile_options(AyraPdfCoreContractTests PRIVATE
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Release>:-O3>)
endif()

target_link_libraries(AyraPdfCoreContractTests PRIVATE
    ayra_pdf
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

include("${AYRA_MODULES}/ayra_pdf/cmake/AyraPdfiumTarget.cmake")
ayra_pdf_link_pdfium(AyraPdfCoreContractTests COPY_RUNTIME)

include(CTest)

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_test(
        NAME ayra_pdf_core_contract
        COMMAND $<TARGET_FILE:AyraPdfCoreContractTests>)
endif()
