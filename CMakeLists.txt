cmake_minimum_required(VERSION 3.20)

# Core Audio process taps (system audio capture) require macOS 14.2
set(CMAKE_OSX_DEPLOYMENT_TARGET "14.2" CACHE STRING "Minimum macOS version" FORCE)
project(eqMax VERSION 1.0.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 6 Setup
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/src/preset
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
)

# Source files
set(SOURCES
    src/main.cpp
    
    # DSP Engine
    src/dsp/BiquadFilter.cpp
    src/dsp/EqualizerEngine.cpp
    src/dsp/ProEffectsEngine.cpp
    src/dsp/SpectrumAnalyzer.cpp
    
    # Audio & CoreAudio HAL Driver Interface
    src/audio/AudioDeviceManager.mm
    src/audio/AppVolumeManager.mm
    
    # Preset System
    src/preset/PresetManager.cpp
    
    # GUI
    src/ui/BasicEQWidget.cpp
    src/ui/EQResponseWidget.cpp
    src/ui/KnobWidget.cpp
    src/ui/PowerButton.cpp
    src/ui/SectionCard.cpp
    src/ui/SpectrumWidget.cpp
    src/ui/VUMeterWidget.cpp
    src/ui/CustomSlider.cpp
    src/ui/MainWindow.cpp
    src/ui/TrayIconManager.cpp
)

set(HEADERS
    src/dsp/BiquadFilter.hpp
    src/dsp/EqualizerEngine.hpp
    src/dsp/ProEffectsEngine.hpp
    src/dsp/SpectrumAnalyzer.hpp
    src/audio/AudioDeviceManager.hpp
    src/audio/AppVolumeManager.hpp
    src/preset/PresetManager.hpp
    src/ui/BasicEQWidget.hpp
    src/ui/EQResponseWidget.hpp
    src/ui/KnobWidget.hpp
    src/ui/PowerButton.hpp
    src/ui/SectionCard.hpp
    src/ui/Theme.hpp
    src/ui/SpectrumWidget.hpp
    src/ui/VUMeterWidget.hpp
    src/ui/CustomSlider.hpp
    src/ui/MainWindow.hpp
    src/ui/TrayIconManager.hpp
)

add_executable(eqMax ${SOURCES} ${HEADERS})

target_compile_features(eqMax PUBLIC cxx_std_20)

target_link_libraries(eqMax PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if(APPLE)
    find_library(COREAUDIO_LIB CoreAudio REQUIRED)
    find_library(AUDIOTOOLBOX_LIB AudioToolbox REQUIRED)
    find_library(ACCELERATE_LIB Accelerate REQUIRED)
    find_library(COCOA_LIB Cocoa REQUIRED)
    find_library(AUDIOUNIT_LIB AudioUnit REQUIRED)
    find_library(AVFOUNDATION_LIB AVFoundation REQUIRED)

    target_link_libraries(eqMax PRIVATE
        ${COREAUDIO_LIB}
        ${AUDIOTOOLBOX_LIB}
        ${ACCELERATE_LIB}
        ${COCOA_LIB}
        ${AUDIOUNIT_LIB}
        ${AVFOUNDATION_LIB}
    )

    target_compile_options(eqMax PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)

    # Set macOS bundle properties
    target_sources(eqMax PRIVATE macOS/AppIcon.icns)
    set_source_files_properties(macOS/AppIcon.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(eqMax PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "eqMax Pro"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.eqmax.equalizer"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/macOS/Info.plist.in
    )
endif()

# Re-sign the whole bundle (ad-hoc) so Info.plist is sealed; macOS privacy (TCC) prompts
# for system audio capture rely on a valid bundle signature.
if(APPLE)
    find_program(CODESIGN_EXECUTABLE codesign)
    if(CODESIGN_EXECUTABLE)
        add_custom_command(TARGET eqMax POST_BUILD
            COMMAND ${CODESIGN_EXECUTABLE} --force --sign - $<TARGET_BUNDLE_DIR:eqMax>
            COMMENT "Ad-hoc signing eqMax.app"
        )
    endif()
endif()

install(TARGETS eqMax
    BUNDLE DESTINATION Applications
    RUNTIME DESTINATION bin
)

# Compiler warnings
if(MSVC)
    target_compile_options(eqMax PRIVATE /W4)
else()
    target_compile_options(eqMax PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Enable CTest
enable_testing()
add_subdirectory(tests)
