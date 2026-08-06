include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

juce_add_console_app(HdnRingmodTests
    PRODUCT_NAME "HDN Ring Modulator Tests"
)

target_sources(HdnRingmodTests PRIVATE
    TestOscillator.cpp
    TestYinPitchDetector.cpp
    TestPitchSmoother.cpp
    TestParameters.cpp
    TestPluginLookAndFeel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/PluginLookAndFeel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/dsp/Oscillator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/dsp/YinPitchDetector.cpp
)

target_include_directories(HdnRingmodTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../source
)

target_compile_definitions(HdnRingmodTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(HdnRingmodTests PRIVATE
    HdnRingmodAssets
    juce::juce_dsp
    juce::juce_gui_basics
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(HdnRingmodTests)
