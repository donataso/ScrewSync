cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(screwsync_unit_tests)

get_filename_component(FIRMWARE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." REALPATH)
get_filename_component(REPO_DIR "${FIRMWARE_DIR}/.." REALPATH)

target_sources(app PRIVATE
    src/test_stepper_ramp.cpp
    src/test_sync_ratio.cpp
    src/test_jog_ratio.cpp
    src/test_stepper_limits.cpp
    src/test_rpm_calc.cpp
    src/test_thread_table.cpp
    src/test_gcode.cpp
    src/test_gcode_meta.cpp
    src/test_probe_cycle.cpp
    src/test_mode_cells.cpp
    src/test_mode_pass_plan.cpp
    src/test_mode_param_codec.cpp
    src/test_mode_math.cpp
    src/test_encoder_filter.cpp
    src/test_display_text.cpp
    src/test_web_text.cpp
    src/test_nextion.cpp
    src/test_thread_table_json.cpp
    src/test_settings_fields.cpp
    src/test_json_span.cpp
    src/test_pin_map.cpp
    src/test_switch_debounce.cpp
    src/test_jog_step.cpp
    src/test_jog_button.cpp
    ${FIRMWARE_DIR}/lib/Stepper/StepperRamp.cpp
    ${FIRMWARE_DIR}/lib/Stepper/StepperLimits.cpp
    ${FIRMWARE_DIR}/lib/Rpm/RpmCalc.cpp
    ${FIRMWARE_DIR}/lib/Sync/SyncRatio.cpp
    ${FIRMWARE_DIR}/lib/Jog/JogRatio.cpp
    ${FIRMWARE_DIR}/lib/Jog/JogStep.cpp
    ${FIRMWARE_DIR}/lib/Thread/ThreadTable.cpp
    ${FIRMWARE_DIR}/lib/Gcode/GcodeCore.cpp
    ${FIRMWARE_DIR}/lib/Gcode/GcodeLathecode.cpp
    ${FIRMWARE_DIR}/lib/Gcode/GcodeFlavors.cpp
    ${FIRMWARE_DIR}/lib/GcodeStore/GcodeMeta.cpp
    ${FIRMWARE_DIR}/lib/Mode/ProbeCycle.cpp
    ${FIRMWARE_DIR}/lib/Display/ModeCells.cpp
    ${FIRMWARE_DIR}/lib/Display/DisplayText.cpp
    ${FIRMWARE_DIR}/lib/Mode/ModePassPlan.cpp
    ${FIRMWARE_DIR}/lib/Mode/ModeParamCodec.cpp
    ${FIRMWARE_DIR}/lib/Mode/ModeMath.cpp
    ${FIRMWARE_DIR}/lib/Encoder/EncoderFilter.cpp
    ${FIRMWARE_DIR}/lib/WebUi/WebText.cpp
    ${FIRMWARE_DIR}/lib/Nextion/NextionFrame.cpp
    ${FIRMWARE_DIR}/lib/Nextion/NextionAttrs.cpp
    ${FIRMWARE_DIR}/lib/Thread/ThreadTableJson.cpp
    ${FIRMWARE_DIR}/lib/Settings/SettingsFields.cpp
    ${FIRMWARE_DIR}/lib/Backup/JsonSpan.cpp
    ${FIRMWARE_DIR}/lib/Pins/PinMap.cpp
    ${FIRMWARE_DIR}/lib/Pins/PinMapJson.cpp
)

target_include_directories(app PRIVATE
    ${FIRMWARE_DIR}/lib/Pins
    ${FIRMWARE_DIR}/lib/Switches
    ${FIRMWARE_DIR}/lib/Stepper
    ${FIRMWARE_DIR}/lib/Sync
    ${FIRMWARE_DIR}/lib/Jog
    ${FIRMWARE_DIR}/lib/Rpm
    ${FIRMWARE_DIR}/lib/Thread
    ${FIRMWARE_DIR}/lib/Gcode
    ${FIRMWARE_DIR}/lib/GcodeStore
    ${FIRMWARE_DIR}/lib/Mode
    ${FIRMWARE_DIR}/lib/Probe
    ${FIRMWARE_DIR}/lib/Display
    ${FIRMWARE_DIR}/lib/Encoder
    ${FIRMWARE_DIR}/lib/WebUi
    ${FIRMWARE_DIR}/lib/Nextion
    ${FIRMWARE_DIR}/lib/Settings
    ${FIRMWARE_DIR}/lib/Backup
    ${FIRMWARE_DIR}/src
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(app PRIVATE -Werror)

# Gcode test vectors (tests/gcode/<flavor>/, shared with the web parser),
# bundled into one embedded file with "=== <name>.<ext>" separators.
set(GCODE_LATHECODE_DIR ${REPO_DIR}/tests/gcode/lathecode)
file(GLOB GCODE_LATHECODE_GCODE_FILES CONFIGURE_DEPENDS ${GCODE_LATHECODE_DIR}/*.gcode)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${GCODE_LATHECODE_GCODE_FILES})

# Written as a C byte list (not via file(READ) text) so CRLF survives.
set(GCODE_LATHECODE_HEX "")
foreach(gcodeFile ${GCODE_LATHECODE_GCODE_FILES})
    get_filename_component(vectorName ${gcodeFile} NAME_WE)
    set(stepsFile ${GCODE_LATHECODE_DIR}/${vectorName}.steps)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${stepsFile})
    foreach(ext gcode steps)
        string(HEX "=== ${vectorName}.${ext}\n" headerHex)
        file(READ ${GCODE_LATHECODE_DIR}/${vectorName}.${ext} bodyHex HEX)
        string(HEX "\n" newlineHex)
        string(APPEND GCODE_LATHECODE_HEX "${headerHex}${bodyHex}${newlineHex}")
    endforeach()
endforeach()
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," GCODE_LATHECODE_BYTES "${GCODE_LATHECODE_HEX}")
file(WRITE ${CMAKE_BINARY_DIR}/include/gcode_lathecode_vectors.inc "${GCODE_LATHECODE_BYTES}\n")
