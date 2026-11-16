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
    src/test_homing_seq.cpp
    src/test_wcs_rules.cpp
    src/test_rpm_calc.cpp
    src/test_thread_table.cpp
    src/test_gcode.cpp
    src/test_gcode_meta.cpp
    src/test_gcode_mdi.cpp
    src/test_gcode_ngc.cpp
    src/test_mode_cells.cpp
    src/test_mode_pass_plan.cpp
    src/test_mode_param_codec.cpp
    src/test_mode_math.cpp
    src/test_mode_mdi_queue.cpp
    src/test_encoder_filter.cpp
    src/test_display_text.cpp
    src/test_web_text.cpp
    src/test_nextion.cpp
    src/test_thread_table_json.cpp
    src/test_settings_fields.cpp
    src/test_json_span.cpp
    src/test_pin_map.cpp
    src/test_input_subscribers.cpp
    src/test_switch_debounce.cpp
    src/test_jog_step.cpp
    src/test_jog_button.cpp
    ${FIRMWARE_DIR}/lib/Stepper/StepperRamp.cpp
    ${FIRMWARE_DIR}/lib/Stepper/StepperLimits.cpp
    ${FIRMWARE_DIR}/lib/Homing/HomingSeq.cpp
    ${FIRMWARE_DIR}/lib/Wcs/WcsRules.cpp
    ${FIRMWARE_DIR}/lib/Rpm/RpmCalc.cpp
    ${FIRMWARE_DIR}/lib/Sync/SyncRatio.cpp
    ${FIRMWARE_DIR}/lib/Jog/JogRatio.cpp
    ${FIRMWARE_DIR}/lib/Jog/JogStep.cpp
    ${FIRMWARE_DIR}/lib/Thread/ThreadTable.cpp
    ${FIRMWARE_DIR}/lib/Gcode/GcodeNgc.cpp
    ${FIRMWARE_DIR}/lib/Gcode/GcodeProgram.cpp
    ${FIRMWARE_DIR}/lib/Gcode/GcodeInterpreter.cpp
    ${FIRMWARE_DIR}/lib/Gcode/GcodeLathecode.cpp
    ${FIRMWARE_DIR}/lib/Gcode/GcodeDialects.cpp
    ${FIRMWARE_DIR}/lib/Gcode/GcodeMdi.cpp
    ${FIRMWARE_DIR}/lib/GcodeStore/GcodeMeta.cpp
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
    ${FIRMWARE_DIR}/lib/Homing
    ${FIRMWARE_DIR}/lib/Wcs
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

# Gcode test vectors (tests/gcode/<dialect>/, shared with the web parser),
# bundled into one embedded file with "=== <dialect>/<name>.<ext>" separators.
set(GCODE_VECTORS_DIR ${REPO_DIR}/tests/gcode)
file(GLOB GCODE_GCODE_FILES CONFIGURE_DEPENDS ${GCODE_VECTORS_DIR}/*/*.gcode)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${GCODE_GCODE_FILES})

# Written as a C byte list (not via file(READ) text) so CRLF survives.
set(GCODE_VECTORS_HEX "")
foreach(gcodeFile ${GCODE_GCODE_FILES})
    get_filename_component(vectorName ${gcodeFile} NAME_WE)
    get_filename_component(vectorDir ${gcodeFile} DIRECTORY)
    get_filename_component(dialect ${vectorDir} NAME)
    set(stepsFile ${vectorDir}/${vectorName}.steps)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${stepsFile})
    foreach(ext gcode steps)
        string(HEX "=== ${dialect}/${vectorName}.${ext}\n" headerHex)
        file(READ ${vectorDir}/${vectorName}.${ext} bodyHex HEX)
        string(HEX "\n" newlineHex)
        string(APPEND GCODE_VECTORS_HEX "${headerHex}${bodyHex}${newlineHex}")
    endforeach()
endforeach()
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," GCODE_VECTORS_BYTES "${GCODE_VECTORS_HEX}")
file(WRITE ${CMAKE_BINARY_DIR}/include/gcode_vectors.inc "${GCODE_VECTORS_BYTES}\n")

foreach(routine probe zero backlash)
    generate_inc_file_for_target(app ${FIRMWARE_DIR}/lib/GcodeStore/routines/${routine}.ngc
        ${CMAKE_BINARY_DIR}/include/routine_${routine}.inc)
endforeach()
