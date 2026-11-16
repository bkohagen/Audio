kit_add_plugin(__TARGET__
    PRODUCT_NAME    "__PRODUCT_NAME__"
    PLUGIN_CODE     __CODE__
    VST3_CATEGORIES __VST3_CATEGORIES__
    AAX_CATEGORY    __AAX_CATEGORY__
    SOURCES         Source/PluginProcessor.cpp
                    Source/PluginEditor.cpp
    DSP_TESTS       tests/dsp_tests.cpp)
