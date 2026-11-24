cmake_minimum_required(VERSION 3.22)

project(dx-rhythmbox VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # Enforce UTF-8 source/execution charset on non-UTF-8 Windows locales (e.g. CP932)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/utf-8>)
endif()

add_library(dx_rhythmbox_dsp INTERFACE)
target_include_directories(dx_rhythmbox_dsp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
target_compile_features(dx_rhythmbox_dsp INTERFACE cxx_std_20)

function(dx_rhythmbox_apply_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
    endif()
endfunction()

option(DX_RHYTHMBOX_BUILD_TESTS "Build Catch2 DSP/Plugin unit tests and parity CLI" ON)
option(DX_RHYTHMBOX_BUILD_PLUGIN "Build JUCE 8 VST3/AU/Standalone plugin and plugin tests" ON)

include(FetchContent)

if(DX_RHYTHMBOX_BUILD_PLUGIN)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.6
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(DXRhythmbox
        COMPANY_NAME "Yuguen Organ"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        BUNDLE_ID "com.yuguenorgan.dxrhythmbox"
        PLUGIN_MANUFACTURER_CODE YgOr
        PLUGIN_CODE DXRb
        FORMATS VST3 AU Standalone
        PRODUCT_NAME "DX Rhythmbox"
        VST3_AUTO_MANIFEST FALSE
        VST3_CATEGORIES "Instrument" "Drum"
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon_1024.png"
        ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon_256.png"
    )

    target_sources(DXRhythmbox PRIVATE
        src/plugin/DXRhythmboxProcessor.cpp
        src/plugin/DXRhythmboxEditor.cpp
        src/ui/DXRhythmboxPanel.cpp
    )

    target_compile_features(DXRhythmbox PUBLIC cxx_std_20)

    target_compile_definitions(DXRhythmbox PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
    )

    target_link_libraries(DXRhythmbox PRIVATE
        dx_rhythmbox_dsp
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
    )

    # Suppress third-party JUCE 8.0.6 internal Apple SDK / HarfBuzz warnings on modern Xcode
    if(APPLE)
        target_compile_options(DXRhythmbox PUBLIC
            -Wno-unknown-warning-option
            -Wno-nontrivial-memcall
            -Wno-deprecated-declarations
        )
    endif()

    # Apply strict warnings only to our own plugin/UI C++ source files
    if(MSVC)
        set_source_files_properties(
            src/plugin/DXRhythmboxProcessor.cpp
            src/plugin/DXRhythmboxEditor.cpp
            src/ui/DXRhythmboxPanel.cpp
            PROPERTIES COMPILE_OPTIONS /W4)
    else()
        set_source_files_properties(
            src/plugin/DXRhythmboxProcessor.cpp
            src/plugin/DXRhythmboxEditor.cpp
            src/ui/DXRhythmboxPanel.cpp
            PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic;-Wshadow")
    endif()
endif()

if(DX_RHYTHMBOX_BUILD_TESTS)
    include(CTest)

    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)

    add_executable(dx_rhythmbox_dsp_tests tests/test_dx_rhythmbox_dsp.cpp)
    target_link_libraries(dx_rhythmbox_dsp_tests PRIVATE dx_rhythmbox_dsp Catch2::Catch2WithMain)
    dx_rhythmbox_apply_warnings(dx_rhythmbox_dsp_tests)
    catch_discover_tests(dx_rhythmbox_dsp_tests)

    add_executable(dx_rhythmbox_render_cli tests/dx_rhythmbox_render_cli.cpp)
    target_link_libraries(dx_rhythmbox_render_cli PRIVATE dx_rhythmbox_dsp)
    dx_rhythmbox_apply_warnings(dx_rhythmbox_render_cli)

    if(DX_RHYTHMBOX_BUILD_PLUGIN)
        add_executable(dx_rhythmbox_plugin_tests tests/test_dx_rhythmbox_plugin.cpp)
        target_link_libraries(dx_rhythmbox_plugin_tests PRIVATE
            DXRhythmbox
            dx_rhythmbox_dsp
            juce::juce_audio_utils
            juce::juce_gui_extra
            Catch2::Catch2WithMain
        )
        target_compile_definitions(dx_rhythmbox_plugin_tests PRIVATE
            DX_RHYTHMBOX_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        )
        dx_rhythmbox_apply_warnings(dx_rhythmbox_plugin_tests)
        catch_discover_tests(dx_rhythmbox_plugin_tests)
    endif()
endif()
