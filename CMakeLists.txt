cmake_minimum_required(VERSION 3.22)

# Universal binary for Apple silicon (arm64) + Intel; must be set before project()
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures")
endif()

project(VBC1 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(VBC_BUILD_TESTS "Build DSP unit tests" ON)

# JUCE: use a local checkout with -DJUCE_SOURCE_DIR=/path/to/JUCE, otherwise download it.
set(JUCE_SOURCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(VBC_FORMATS VST3 AU)
else()
    set(VBC_FORMATS VST3)
endif()

juce_add_plugin(VBC1
    COMPANY_NAME                "VBC Audio"
    BUNDLE_ID                   "com.vbcaudio.vbc1"
    PLUGIN_MANUFACTURER_CODE    Vbca
    PLUGIN_CODE                 Vbc1
    PRODUCT_NAME                "VBC-1 Bus Compressor"
    FORMATS                     ${VBC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Dynamics
    AU_MAIN_TYPE                kAudioUnitType_Effect
    HARDENED_RUNTIME_ENABLED    TRUE)

target_sources(VBC1 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(VBC1 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VBC1
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(VBC_BUILD_TESTS)
    enable_testing()
    add_executable(vbc_dsp_tests tests/dsp_tests.cpp)
    add_test(NAME vbc_dsp_tests COMMAND vbc_dsp_tests)

    # Real-time safety test: traps malloc/new/free/mutex calls made on the audio thread.
    # Uses GNU ld --wrap, so it runs on Linux (CI runs it on every push).
    if(UNIX AND NOT APPLE)
        juce_add_console_app(vbc_rt_safety_test PRODUCT_NAME "vbc_rt_safety_test")
        target_sources(vbc_rt_safety_test PRIVATE tests/rt_safety_test.cpp)
        target_link_libraries(vbc_rt_safety_test PRIVATE VBC1 juce::juce_audio_utils)
        target_compile_definitions(vbc_rt_safety_test PRIVATE
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_compile_options(vbc_rt_safety_test PRIVATE -fno-pie)
        target_link_options(vbc_rt_safety_test PRIVATE -no-pie -rdynamic
            -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free,--wrap=posix_memalign,--wrap=aligned_alloc
            -Wl,--wrap=pthread_mutex_lock,--wrap=pthread_mutex_trylock
            -Wl,--wrap=pthread_rwlock_rdlock,--wrap=pthread_rwlock_wrlock
            -Wl,--wrap=pthread_cond_wait,--wrap=pthread_cond_timedwait)
        add_test(NAME vbc_rt_safety_test COMMAND vbc_rt_safety_test)
    endif()
endif()
