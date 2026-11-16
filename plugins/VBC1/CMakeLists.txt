kit_add_plugin(VBC1
    PRODUCT_NAME    "VBC-1 Bus Compressor"
    PLUGIN_CODE     Vbc1
    VST3_CATEGORIES Fx Dynamics
    AAX_CATEGORY    AAX_ePlugInCategory_Dynamics
    SOURCES         Source/PluginProcessor.cpp
                    Source/PluginEditor.cpp
    DSP_TESTS       tests/dsp_tests.cpp)
