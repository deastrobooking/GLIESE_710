cmake_minimum_required(VERSION 3.22)
project(GLIESE_710 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GLIESE_COPY_PLUGIN_AFTER_BUILD "Copy built plugin artefacts into system plugin folders" OFF)
option(GLIESE_BUILD_CLAP "Build the CLAP plugin wrapper through clap-juce-extensions" ON)
option(GLIESE_NATIVE_OPTIMIZATIONS "Use host-native CPU tuning for local benchmarks; keep OFF for distributable universal builds" OFF)

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.tar.gz
)

if(GLIESE_BUILD_CLAP)
    set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    set(GLIESE_CLAP_JUCE_EXTENSIONS_COMMIT e8de9e8571626633b8541a54c2406fccc4272767)
    set(GLIESE_CLAP_CORE_COMMIT 29ffcc273be7c7c651f6c9953b99e69700e2387a)
    set(GLIESE_CLAP_HELPERS_COMMIT a61bcdf0ecc2c8db1e80bfe8bf9cb7e8d9fd2bbc)

    CPMAddPackage(
        NAME clap_core
        URL https://github.com/free-audio/clap/archive/${GLIESE_CLAP_CORE_COMMIT}.tar.gz
        DOWNLOAD_ONLY YES
    )

    CPMAddPackage(
        NAME clap_helpers
        URL https://github.com/free-audio/clap-helpers/archive/${GLIESE_CLAP_HELPERS_COMMIT}.tar.gz
        DOWNLOAD_ONLY YES
    )

    CPMAddPackage(
        NAME clap_juce_extensions
        # Release tags predate the current JUCE 8 wrapper support.
        URL https://github.com/free-audio/clap-juce-extensions/archive/${GLIESE_CLAP_JUCE_EXTENSIONS_COMMIT}.tar.gz
        DOWNLOAD_ONLY YES
    )

    set(GLIESE_CLAP_LIBS_DIR "${clap_juce_extensions_SOURCE_DIR}/clap-libs")
    file(REMOVE_RECURSE
        "${GLIESE_CLAP_LIBS_DIR}/clap"
        "${GLIESE_CLAP_LIBS_DIR}/clap-helpers"
    )
    file(MAKE_DIRECTORY "${GLIESE_CLAP_LIBS_DIR}")
    file(COPY "${clap_core_SOURCE_DIR}/" DESTINATION "${GLIESE_CLAP_LIBS_DIR}/clap")
    file(COPY "${clap_helpers_SOURCE_DIR}/" DESTINATION "${GLIESE_CLAP_LIBS_DIR}/clap-helpers")

    add_subdirectory("${clap_juce_extensions_SOURCE_DIR}" "${clap_juce_extensions_BINARY_DIR}")
endif()

# ── Plugin target ──────────────────────────────────────────────────────────────
juce_add_plugin(GLIESE_710
    VERSION                     ${PROJECT_VERSION}
    COMPANY_NAME                "ECOSYSTEM"
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
    Source/Engine/Oscillators/KarplusStrong.cpp
    Source/Engine/Oscillators/GranularOsc.cpp
    Source/Engine/Oscillators/TerrainOsc.cpp
    Source/Engine/Envelopes/ADSREnvelope.cpp
    Source/Engine/Envelopes/EnvelopeBank.cpp
    Source/Engine/LFO/LFOEngine.cpp
    Source/Engine/Filter/DualFilter.cpp
    Source/Engine/Modulation/ModMatrix.cpp
    Source/Engine/Modulation/StepSequencerMod.cpp
    Source/Engine/Arpeggiator/Arpeggiator.cpp
    Source/Presets/PresetManager.cpp
    Source/UI/MainPanel.cpp
    Source/UI/IndustrialTheme.cpp
    Source/UI/OscPanel.cpp
    Source/UI/FilterPanel.cpp
    Source/UI/ModMatrixPanel.cpp
    Source/UI/ModStepSequencerPanel.cpp
    Source/UI/ArpPanel.cpp
    Source/UI/LFOPanel.cpp
    Source/UI/EngineSlotBar.cpp
    Source/UI/PresetBrowser.cpp
    Source/UI/PatchBrowserPanel.cpp
)

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(GLIESE_710
PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
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

target_compile_options(GLIESE_710 PRIVATE
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:AppleClang,Clang,GNU>>:-O3;-fomit-frame-pointer>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/O2>
)

if(GLIESE_NATIVE_OPTIMIZATIONS)
    target_compile_options(GLIESE_710 PRIVATE
        $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:AppleClang,Clang,GNU>>:-march=native>
        $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/arch:AVX2>
    )
endif()

if(GLIESE_BUILD_CLAP)
    clap_juce_extensions_plugin(TARGET GLIESE_710
        CLAP_ID "com.gliesestudios.gliese710"
        CLAP_FEATURES instrument synthesizer stereo
        CLAP_USE_JUCE_PARAMETER_RANGES ALL
    )

    if(TARGET GLIESE_710_All AND TARGET GLIESE_710_CLAP)
        add_dependencies(GLIESE_710_All GLIESE_710_CLAP)
    endif()
endif()

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
        Source/Engine/Oscillators/GranularOsc.cpp
        Source/Engine/Oscillators/TerrainOsc.cpp
        Source/Engine/Filter/DualFilter.cpp
        Source/Engine/Modulation/ModMatrix.cpp
        Source/Engine/Modulation/StepSequencerMod.cpp
        Source/Engine/LFO/LFOEngine.cpp
        Source/Engine/Oscillators/VAOscillator.cpp
        Source/Engine/Oscillators/KarplusStrong.cpp
        Source/Engine/Arpeggiator/Arpeggiator.cpp
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
