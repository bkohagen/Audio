# kit_add_plugin(): one call per plugin. Gives every plugin the same formats, identity scheme,
# compile settings, shared UI/DSP kit, DSP unit tests, real-time-safety test and screenshot tool.
#
#   kit_add_plugin(<TARGET>
#       PRODUCT_NAME   "My Plugin"          # name shown in hosts
#       PLUGIN_CODE    Abcd                 # 4 chars, exactly 1 uppercase, UNIQUE per plugin, never change
#       SOURCES        Source/PluginProcessor.cpp Source/PluginEditor.cpp
#       [DSP_TESTS     tests/dsp_tests.cpp] # framework-free tests, run by ctest
#       [VST3_CATEGORIES Fx Dynamics]       # default: Fx
#       [AU_MAIN_TYPE  kAudioUnitType_Effect]
#       [AAX_CATEGORY  AAX_ePlugInCategory_Dynamics]   # default: AAX_ePlugInCategory_None
#       [IS_SYNTH])                         # instruments: adds MIDI input
#
# Formats: VST3 everywhere, AU on macOS, AAX (Pro Tools) when KIT_AAX_SDK_PATH points at the
# Avid AAX SDK (macOS/Windows only).
include_guard(GLOBAL)

set(KIT_COMPANY_NAME      "VBC Audio" CACHE STRING "Company name shown in hosts")
set(KIT_MANUFACTURER_CODE "Vbca"      CACHE STRING "4-char manufacturer code (1 uppercase). NEVER change: hosts key saved sessions on it")
set(KIT_BUNDLE_PREFIX     "com.vbcaudio" CACHE STRING "Reverse-DNS prefix for bundle ids")

# ---- Pro Tools (AAX) ---------------------------------------------------------------------------
set(KIT_AAX_SDK_PATH "$ENV{AAX_SDK_PATH}" CACHE PATH "Path to the Avid AAX SDK (enables the AAX format for Pro Tools)")
set(KIT_AAX_ENABLED OFF)
if(KIT_AAX_SDK_PATH AND (APPLE OR WIN32))
    if(EXISTS "${KIT_AAX_SDK_PATH}/Interfaces/AAX.h")
        juce_set_aax_sdk_path("${KIT_AAX_SDK_PATH}")
        set(KIT_AAX_ENABLED ON)
        message(STATUS "AAX (Pro Tools) enabled: ${KIT_AAX_SDK_PATH}")
    else()
        message(WARNING "KIT_AAX_SDK_PATH='${KIT_AAX_SDK_PATH}' does not look like the AAX SDK (no Interfaces/AAX.h); AAX disabled")
    endif()
endif()

function(kit_add_plugin target)
    cmake_parse_arguments(ARG "IS_SYNTH" "PRODUCT_NAME;PLUGIN_CODE;AU_MAIN_TYPE;AAX_CATEGORY"
                                         "SOURCES;DSP_TESTS;VST3_CATEGORIES" ${ARGN})
    foreach(req PRODUCT_NAME PLUGIN_CODE SOURCES)
        if(NOT ARG_${req})
            message(FATAL_ERROR "kit_add_plugin(${target}): ${req} is required")
        endif()
    endforeach()
    if(NOT ARG_VST3_CATEGORIES)
        set(ARG_VST3_CATEGORIES Fx)
    endif()
    if(NOT ARG_AU_MAIN_TYPE)
        set(ARG_AU_MAIN_TYPE kAudioUnitType_Effect)
        if(ARG_IS_SYNTH)
            set(ARG_AU_MAIN_TYPE kAudioUnitType_MusicDevice)
        endif()
    endif()
    if(NOT ARG_AAX_CATEGORY)
        set(ARG_AAX_CATEGORY AAX_ePlugInCategory_None)
    endif()

    set(formats VST3)
    if(APPLE)
        list(APPEND formats AU)
    endif()
    if(KIT_AAX_ENABLED)
        list(APPEND formats AAX)
    endif()

    set(synth FALSE)
    if(ARG_IS_SYNTH)
        set(synth TRUE)
    endif()

    string(TOLOWER "${target}" lower)
    juce_add_plugin(${target}
        COMPANY_NAME                "${KIT_COMPANY_NAME}"
        BUNDLE_ID                   "${KIT_BUNDLE_PREFIX}.${lower}"
        PLUGIN_MANUFACTURER_CODE    ${KIT_MANUFACTURER_CODE}
        PLUGIN_CODE                 ${ARG_PLUGIN_CODE}
        PRODUCT_NAME                "${ARG_PRODUCT_NAME}"
        FORMATS                     ${formats}
        IS_SYNTH                    ${synth}
        NEEDS_MIDI_INPUT            ${synth}
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        VST3_CATEGORIES             ${ARG_VST3_CATEGORIES}
        AU_MAIN_TYPE                ${ARG_AU_MAIN_TYPE}
        AAX_CATEGORY                ${ARG_AAX_CATEGORY}
        HARDENED_RUNTIME_ENABLED    TRUE)

    target_sources(${target} PRIVATE ${ARG_SOURCES})
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(${target}
        PRIVATE
            kit
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    if(NOT KIT_BUILD_TESTS)
        return()
    endif()

    # ---- DSP unit tests (framework-free, every platform) -----------------------------------
    if(ARG_DSP_TESTS)
        add_executable(${target}_dsp_tests ${ARG_DSP_TESTS})
        target_include_directories(${target}_dsp_tests PRIVATE "${KIT_ROOT}/common" "${CMAKE_CURRENT_SOURCE_DIR}/Source")
        target_compile_features(${target}_dsp_tests PRIVATE cxx_std_17)
        add_test(NAME ${target}_dsp_tests COMMAND ${target}_dsp_tests)
    endif()

    # ---- Linux-only tools: RT-safety test (GNU ld --wrap) and headless screenshot ----------
    if(UNIX AND NOT APPLE)
        juce_add_console_app(${target}_rt_safety_test PRODUCT_NAME "${target}_rt_safety_test")
        target_sources(${target}_rt_safety_test PRIVATE "${KIT_ROOT}/common/kit/tests/rt_safety_test.cpp")
        target_link_libraries(${target}_rt_safety_test PRIVATE ${target} juce::juce_audio_utils)
        target_compile_definitions(${target}_rt_safety_test PRIVATE
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_compile_options(${target}_rt_safety_test PRIVATE -fno-pie)
        target_link_options(${target}_rt_safety_test PRIVATE -no-pie -rdynamic
            -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free,--wrap=posix_memalign,--wrap=aligned_alloc
            -Wl,--wrap=pthread_mutex_lock,--wrap=pthread_mutex_trylock
            -Wl,--wrap=pthread_rwlock_rdlock,--wrap=pthread_rwlock_wrlock
            -Wl,--wrap=pthread_cond_wait,--wrap=pthread_cond_timedwait)
        add_test(NAME ${target}_rt_safety_test COMMAND ${target}_rt_safety_test)

        juce_add_console_app(${target}_screenshot PRODUCT_NAME "${target}_screenshot")
        target_sources(${target}_screenshot PRIVATE "${KIT_ROOT}/common/kit/tools/screenshot.cpp")
        target_link_libraries(${target}_screenshot PRIVATE ${target} juce::juce_audio_utils)
        target_compile_definitions(${target}_screenshot PRIVATE
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    endif()
endfunction()
