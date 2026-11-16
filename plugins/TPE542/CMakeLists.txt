kit_add_plugin(TPE542
    PRODUCT_NAME    "TPE-542 Tape Emulator"
    PLUGIN_CODE     Tp42
    VST3_CATEGORIES Fx Distortion
    AAX_CATEGORY    AAX_ePlugInCategory_Harmonic
    SOURCES         Source/PluginProcessor.cpp
                    Source/PluginEditor.cpp
    DSP_TESTS       tests/dsp_tests.cpp)
