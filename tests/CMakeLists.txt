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
