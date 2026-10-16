# Unit Tests CMakeLists.txt

add_executable(test_dsp
    test_dsp.cpp
    ../src/dsp/BiquadFilter.cpp
    ../src/dsp/EqualizerEngine.cpp
    ../src/dsp/ProEffectsEngine.cpp
    ../src/dsp/SpectrumAnalyzer.cpp
)

target_include_directories(test_dsp PRIVATE ../src/dsp)
target_compile_features(test_dsp PUBLIC cxx_std_20)

if(APPLE)
    find_library(ACCELERATE_LIB Accelerate REQUIRED)
    target_link_libraries(test_dsp PRIVATE ${ACCELERATE_LIB})
endif()

add_test(NAME DSPUnitTest COMMAND test_dsp)

# Assertions must also run in Release builds.
target_compile_options(test_dsp PRIVATE -UNDEBUG)

add_executable(test_presets test_presets.cpp ../src/preset/PresetManager.cpp)
target_link_libraries(test_presets PRIVATE Qt6::Core)
target_compile_options(test_presets PRIVATE -UNDEBUG)
add_test(NAME PresetRoundTrip COMMAND test_presets)

find_package(Qt6 REQUIRED COMPONENTS Test)
set(UI_TEST_SOURCES ${SOURCES})
list(REMOVE_ITEM UI_TEST_SOURCES src/main.cpp)
list(TRANSFORM UI_TEST_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/")
add_executable(test_ui test_ui.cpp ${UI_TEST_SOURCES})
target_link_libraries(test_ui PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
target_compile_options(test_ui PRIVATE -UNDEBUG)
if(APPLE)
    target_compile_options(test_ui PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    target_link_libraries(test_ui PRIVATE ${COREAUDIO_LIB} ${AUDIOTOOLBOX_LIB} ${ACCELERATE_LIB}
        ${COCOA_LIB} ${AUDIOUNIT_LIB} ${AVFOUNDATION_LIB})
endif()
add_test(NAME UIStateAndControls COMMAND test_ui)
set_tests_properties(UIStateAndControls PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
