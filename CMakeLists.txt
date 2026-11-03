cmake_minimum_required(VERSION 3.22)
project(AudioVisualizer VERSION 0.77.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG 7adee3a1bd4684d4caa5601100e364abccff4b4d
    GIT_SHALLOW OFF)
FetchContent_Declare(tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG v2.9.3
    GIT_SHALLOW ON
    SOURCE_SUBDIR _header_only_no_cmake)
FetchContent_MakeAvailable(JUCE clap-juce-extensions tinygltf)

set(AU_FORMAT "")
if(APPLE)
    set(AU_FORMAT AU)
endif()

juce_add_plugin(AudioVisualizer
    COMPANY_NAME "mtyas"
    COMPANY_WEBSITE "https://mtyas.com"
    COMPANY_COPYRIGHT "(c) 2026 mtyas"
    DESCRIPTION "Sound-reactive visuals by mtyas"
    PLUGIN_MANUFACTURER_CODE Avzr
    PLUGIN_CODE Avz1
    FORMATS VST3 ${AU_FORMAT} Standalone
    PRODUCT_NAME "AudioVisualizer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    CAMERA_PERMISSION_ENABLED TRUE
    CAMERA_PERMISSION_TEXT "AudioVisualizer uses the camera to drive visuals.")

clap_juce_extensions_plugin(TARGET AudioVisualizer
    CLAP_ID "org.audiovisualizer.plugin"
    CLAP_FEATURES audio-effect analyzer utility)

target_sources(AudioVisualizer PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/OutputWindow.cpp
    src/core/VisualState.cpp
    src/audio/Analyzer.cpp
    src/video/CameraInput.cpp
    src/video/VideoDecoder.cpp
    src/graph/ModGraph.cpp
    src/scripting/ScriptWasmEngine.cpp
    src/modulation/ModulationSystem.cpp
    src/presets/PresetManager.cpp
    src/render/Renderer.cpp
    src/render/GltfLoader.cpp)

target_include_directories(AudioVisualizer PRIVATE src)
target_include_directories(AudioVisualizer SYSTEM PRIVATE ${tinygltf_SOURCE_DIR})

target_compile_definitions(AudioVisualizer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CAMERA=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AudioVisualizer PRIVATE
    juce::juce_audio_utils
    juce::juce_opengl
    juce::juce_dsp
    juce::juce_video
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
