cmake_minimum_required(VERSION 3.22)
project(GLIESE_710 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GLIESE_COPY_PLUGIN_AFTER_BUILD "Copy built plugin artefacts into system plugin folders" OFF)

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
    BUNDLE_ID                   "com.gliesestudios.GLIESE710"
    PLUGIN_MANUFACTURER_CODE    GLSE
    PLUGIN_CODE                 G710
    LV2URI                      "urn:com:gliesestudios:gliese710"
    FORMATS                     AU VST3 LV2 Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${GLIESE_COPY_PLUGIN_AFTER_BUILD}
    VST3_COPY_DIR               "/Library/Audio/Plug-Ins/VST3"
    AU_COPY_DIR                 "/Library/Audio/Plug-Ins/Components"
    PRODUCT_NAME                "GLIESE 710"
    DESCRIPTION                 "3D Function Oscillator Synthesizer"
    VST3_CATEGORIES             "Instrument|Synth"
)

juce_generate_juce_header(GLIESE_710)

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
    Source/UI/IndustrialTheme.cpp
    Source/UI/OscPanel.cpp
    Source/UI/FilterPanel.cpp
    Source/UI/ModMatrixPanel.cpp
    Source/UI/ArpPanel.cpp
    Source/UI/LFOPanel.cpp
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
    list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")

    juce_add_console_app(GLIESE_Tests PRODUCT_NAME "GLIESE Tests")
    juce_generate_juce_header(GLIESE_Tests)

    target_sources(GLIESE_Tests PRIVATE
        Tests/EngineTests.cpp
        Source/Engine/Envelopes/ADSREnvelope.cpp
        Source/Engine/Oscillators/Osc3D.cpp
        Source/Engine/Oscillators/FMOperator.cpp
        Source/Engine/Oscillators/FMAlgorithm.cpp
        Source/Engine/Oscillators/WavetableOsc.cpp
        Source/Engine/Filter/DualFilter.cpp
        Source/Engine/Modulation/ModMatrix.cpp
        Source/Engine/LFO/LFOEngine.cpp
        Source/Engine/Oscillators/VAOscillator.cpp
    )

    target_include_directories(GLIESE_Tests PRIVATE Source)

    target_compile_definitions(GLIESE_Tests PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )

    target_link_libraries(GLIESE_Tests
        PRIVATE
            Catch2::Catch2WithMain
            juce::juce_dsp
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
    )

    include(CTest)
    include(Catch)
    catch_discover_tests(GLIESE_Tests)
endif()
