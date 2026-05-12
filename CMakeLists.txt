cmake_minimum_required(VERSION 3.22)
project(GLIESE_710 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME JUCE
    GITHUB_REPOSITORY juce-framework/JUCE
    GIT_TAG 8.0.4
)

# ── Plugin target ──────────────────────────────────────────────────────────────
juce_add_plugin(GLIESE_710
    VERSION                     ${PROJECT_VERSION}
    COMPANY_NAME                "GLIESE Studios"
    COMPANY_WEBSITE             ""
    PLUGIN_MANUFACTURER_CODE    GLSE
    PLUGIN_CODE                 G710
    FORMATS                     AU VST3 LV2 Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    VST3_COPY_DIR               "/Library/Audio/Plug-Ins/VST3"
    AU_COPY_DIR                 "/Library/Audio/Plug-Ins/Components"
    PRODUCT_NAME                "GLIESE 710"
    DESCRIPTION                 "3D Function Oscillator Synthesizer"
    VST3_CATEGORIES             "Instrument|Synth"
)

# ── Binary assets ─────────────────────────────────────────────────────────────
# Placeholder single-cycle sine for initial build (real tables added in Phase 6)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets/Wavetables/.gitkeep
    ${CMAKE_CURRENT_BINARY_DIR}/Assets/Wavetables/.gitkeep COPYONLY
)

# ── Sources ───────────────────────────────────────────────────────────────────
target_sources(GLIESE_710
PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Engine/SynthVoice.cpp
    Source/Engine/SynthEngine.cpp
    Source/Engine/Oscillators/Osc3D.cpp
    Source/Engine/Oscillators/FMOperator.cpp
    Source/Engine/Oscillators/FMAlgorithm.cpp
    Source/Engine/Oscillators/WavetableOsc.cpp
    Source/Engine/Oscillators/VAOscillator.cpp
    Source/Engine/Envelopes/ADSREnvelope.cpp
    Source/Engine/Envelopes/EnvelopeBank.cpp
    Source/Engine/LFO/LFOEngine.cpp
    Source/Engine/Filter/DualFilter.cpp
    Source/Engine/Modulation/ModMatrix.cpp
    Source/Engine/Arpeggiator/Arpeggiator.cpp
    Source/Presets/PresetManager.cpp
    Source/UI/MainPanel.cpp
    Source/UI/OscPanel.cpp
    Source/UI/FilterPanel.cpp
    Source/UI/ModMatrixPanel.cpp
    Source/UI/ArpPanel.cpp
    Source/UI/PresetBrowser.cpp
)

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(GLIESE_710
PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# ── Include paths ─────────────────────────────────────────────────────────────
target_include_directories(GLIESE_710 PRIVATE Source)

# ── Link libraries ────────────────────────────────────────────────────────────
target_link_libraries(GLIESE_710
PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_plugin_client
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# ── Tests (optional, requires Catch2) ─────────────────────────────────────────
option(GLIESE_BUILD_TESTS "Build unit tests" OFF)
if(GLIESE_BUILD_TESTS)
    CPMAddPackage("gh:catchorg/Catch2@3.6.0")
    add_executable(GLIESE_Tests Tests/EngineTests.cpp)
    target_link_libraries(GLIESE_Tests PRIVATE Catch2::Catch2WithMain juce::juce_dsp)
    include(CTest)
    include(Catch)
    catch_discover_tests(GLIESE_Tests)
endif()
