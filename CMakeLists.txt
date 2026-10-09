cmake_minimum_required(VERSION 3.22)
project(EQ1 VERSION 0.3.2 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(EQ1_BUILD_PLUGIN "Build native plug-ins and host checks" ON)
enable_testing()
add_executable(EQ1DSPTest Tests/dsp_test.cpp)
target_include_directories(EQ1DSPTest PRIVATE Source)
add_test(NAME dsp COMMAND EQ1DSPTest)
if(MINGW)
    target_link_options(EQ1DSPTest PRIVATE -static)
endif()
if(NOT EQ1_BUILD_PLUGIN)
    return()
endif()
if(NOT DEFINED JUCE_PATH)
    message(FATAL_ERROR "Provide JUCE_PATH (tested with JUCE 7.0.12)")
endif()
add_subdirectory("${JUCE_PATH}" JUCE)
set(CS_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND CS_FORMATS AU)
endif()
juce_add_plugin(EQ1
    COMPANY_NAME "Shaper Audio"
    BUNDLE_ID "audio.shaper.curveshaper"
    PLUGIN_MANUFACTURER_CODE Shpr
    PLUGIN_CODE Csq1
    FORMATS ${CS_FORMATS}
    PRODUCT_NAME "EQ-01"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx EQ
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(EQ1 PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(EQ1 PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(EQ1 PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
if(MINGW)
    target_link_options(EQ1 PUBLIC -static)
endif()
add_executable(EQ1Preview Tests/preview.cpp)
target_include_directories(EQ1Preview PRIVATE Source "${JUCE_PATH}/modules")
target_compile_definitions(EQ1Preview PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
target_link_libraries(EQ1Preview PRIVATE EQ1)
add_executable(EQ1HostCheck Tests/host_check.cpp)
target_compile_definitions(EQ1HostCheck PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_VST=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(EQ1HostCheck PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
if(MINGW)
    target_link_options(EQ1Preview PRIVATE -static)
    target_link_options(EQ1HostCheck PRIVATE -static)
endif()
