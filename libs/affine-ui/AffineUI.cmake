# Affine UI: the shared rendered-instrument design language for Affine plug-ins.
# include() this file after JUCE is available, then link the `affine_ui` target.
include_guard(GLOBAL)

set(AFFINE_UI_ROOT "${CMAKE_CURRENT_LIST_DIR}")

juce_add_binary_data(affine_ui_fonts
    NAMESPACE AffineFonts
    HEADER_NAME AffineFonts.h
    SOURCES
        "${AFFINE_UI_ROOT}/fonts/BarlowCondensed-Regular.ttf"
        "${AFFINE_UI_ROOT}/fonts/BarlowCondensed-SemiBold.ttf"
        "${AFFINE_UI_ROOT}/fonts/Michroma-Regular.ttf"
        "${AFFINE_UI_ROOT}/fonts/ShareTechMono-Regular.ttf")

juce_add_module("${AFFINE_UI_ROOT}/affine_ui")
target_link_libraries(affine_ui INTERFACE affine_ui_fonts)
