cmake_minimum_required(VERSION 3.22)
project(TSRQ VERSION 0.3.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# stessi arrotondamenti su ogni CPU/compilatore (niente FMA automatiche): il progetto dei filtri deve dare lo stesso risultato su Mac arm64, Intel e Linux
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-ffp-contract=off)
endif()
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

# JUCE: -DJUCE_DIR=/percorso/JUCE oppure download automatico della 8.0.4
if(DEFINED JUCE_DIR)
  add_subdirectory(${JUCE_DIR} juce)
else()
  include(FetchContent)
  FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(juce)
endif()

set(TSRQ_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND TSRQ_FORMATS AU)
endif()
# AAX (Pro Tools): serve l'AAX SDK di Avid (non incluso, si scarica dall'account sviluppatore Avid): -DAAX_SDK_PATH=/percorso/aax-sdk
if(DEFINED AAX_SDK_PATH)
  if(NOT EXISTS "${AAX_SDK_PATH}/Interfaces/AAX.h")
    message(FATAL_ERROR "AAX_SDK_PATH non valido: manca ${AAX_SDK_PATH}/Interfaces/AAX.h")
  endif()
  juce_set_aax_sdk_path("${AAX_SDK_PATH}")
  list(APPEND TSRQ_FORMATS AAX)
endif()

juce_add_plugin(TSRQ
  COMPANY_NAME "TSR Audio"
  BUNDLE_ID "com.tsraudio.tsrq"
  PLUGIN_MANUFACTURER_CODE Tsra
  PLUGIN_CODE Tsrq
  FORMATS ${TSRQ_FORMATS}
  PRODUCT_NAME "TSR Q"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  NEEDS_WEB_BROWSER TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  AU_MAIN_TYPE kAudioUnitType_Effect
  VST3_CATEGORIES Fx EQ
  AAX_CATEGORY AAX_ePlugInCategory_EQ
  AAX_IDENTIFIER com.tsraudio.tsrq)

juce_add_binary_data(TSRQWeb SOURCES web/index.html)
target_sources(TSRQ PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/WebEditor.cpp)
target_include_directories(TSRQ PRIVATE Source)
target_compile_definitions(TSRQ PUBLIC
  JUCE_WEB_BROWSER=1 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TSRQ PRIVATE TSRQWeb juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

add_executable(tsrq_dsp_tests tests/dsp_tests.cpp)
target_include_directories(tsrq_dsp_tests PRIVATE Source)

add_executable(tsrq_stem_tests tests/stem_tests.cpp)
target_include_directories(tsrq_stem_tests PRIVATE Source)
