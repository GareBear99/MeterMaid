cmake_minimum_required(VERSION 3.22)
project(MeterMaid VERSION 0.7.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE setup:
# Provide JUCE_DIR (folder containing JUCEConfig.cmake) OR JUCE_PATH (path to JUCE repo).
# Example:
#   cmake -B build -S . -DJUCE_DIR=/path/to/JUCE
# or:
#   cmake -B build -S . -DJUCE_PATH=../JUCE
if (DEFINED JUCE_DIR)
  list(APPEND CMAKE_PREFIX_PATH "${JUCE_DIR}")
  find_package(JUCE CONFIG REQUIRED)
elseif(DEFINED JUCE_PATH)
  add_subdirectory("${JUCE_PATH}" JUCE)
else()
  message(FATAL_ERROR "Please set JUCE_DIR or JUCE_PATH to your local JUCE folder.")
endif()

juce_add_plugin(MeterMaid
    COMPANY_NAME "TizWildin Entertainment"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE TWE1
    PLUGIN_CODE MtrM
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "MeterMaid")

target_sources(MeterMaid PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/MeterEngine.h
    Source/MeterFrame.h
    Source/LockFreeFramePipe.h)

target_compile_definitions(MeterMaid PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(MeterMaid PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra)

# Auto-added: generate JuceHeader.h for sources that include <JuceHeader.h>
juce_generate_juce_header(MeterMaid)

target_compile_definitions(MeterMaid PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
