cmake_minimum_required(VERSION 3.25...3.31)

cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)

project(HdnRingmod VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_binary_data(HdnRingmodAssets
    NAMESPACE HdnRingmodAssets
    HEADER_NAME HdnRingmodAssets.h
    SOURCES
        assets/fonts/BarlowCondensed-Regular.ttf
        assets/fonts/BarlowCondensed-SemiBold.ttf
)

juce_add_plugin(HdnRingmod
    COMPANY_NAME "HDN"
    PLUGIN_MANUFACTURER_CODE Hdnx
    PLUGIN_CODE Hdrm
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "HDN Ring Modulator"
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

add_library(HdnRingmodShared INTERFACE)

target_sources(HdnRingmodShared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/source/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/PluginLookAndFeel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/YinPitchDetector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/Oscillator.cpp
)

target_include_directories(HdnRingmodShared INTERFACE source)

target_link_libraries(HdnRingmodShared INTERFACE
    HdnRingmodAssets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
)

target_link_libraries(HdnRingmod PRIVATE HdnRingmodShared)

target_compile_definitions(HdnRingmod PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

option(HDN_BUILD_TESTS "Build unit tests" OFF)
if(HDN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
