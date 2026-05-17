cmake_minimum_required(VERSION 3.22)

project(CRSR VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    add_subdirectory(external/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Uclid — Euclidean volume gate (VST3 + Standalone)
# ---------------------------------------------------------------------------
juce_add_plugin(Uclid
    COMPANY_NAME "Uclid"
    BUNDLE_ID com.uclid.plugin
    PLUGIN_MANUFACTURER_CODE Ucld
    PLUGIN_CODE Ucl1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Uclid"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# Final build output (use these paths for FL / distribution).
set(UCLID_FINAL_DIR "${CMAKE_SOURCE_DIR}/final")

add_custom_command(TARGET Uclid_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${UCLID_FINAL_DIR}/VST3/Uclid.vst3"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${UCLID_FINAL_DIR}/VST3"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/Uclid_artefacts/$<CONFIG>/VST3/Uclid.vst3"
        "${UCLID_FINAL_DIR}/VST3/Uclid.vst3"
    COMMENT "Copying Uclid.vst3 to final/VST3"
)

add_custom_command(TARGET Uclid_Standalone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${UCLID_FINAL_DIR}/Standalone"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:Uclid_Standalone>"
        "${UCLID_FINAL_DIR}/Standalone/Uclid.exe"
    COMMENT "Copying Uclid.exe to final/Standalone"
)

juce_generate_juce_header(Uclid)

target_sources(Uclid
    PRIVATE
        Uclid/Source/PluginProcessor.cpp
        Uclid/Source/PluginEditor.cpp
)

target_compile_definitions(Uclid
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Uclid
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
)

# ---------------------------------------------------------------------------
# CRSR demo app (optional)
# ---------------------------------------------------------------------------
juce_add_gui_app(CRSRApp
    COMPANY_NAME "CRSR"
    PRODUCT_NAME "CRSRApp"
    VERSION ${PROJECT_VERSION}
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE
)

juce_generate_juce_header(CRSRApp)

target_sources(CRSRApp
    PRIVATE
        Source/Main.cpp
        Source/MainComponent.cpp
)

target_compile_definitions(CRSRApp
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CRSRApp,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CRSRApp,JUCE_VERSION>"
)

target_link_libraries(CRSRApp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
)
