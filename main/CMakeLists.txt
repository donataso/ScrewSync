cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(screwsync)

target_sources(app PRIVATE
    src/main.cpp
    lib/Console/Console.cpp
    lib/Command/Command.cpp
    lib/Command/CommandWifi.cpp
    lib/Command/CommandSystem.cpp
    lib/Command/CommandMotion.cpp
    lib/Command/CommandMode.cpp
    lib/Motion/Motion.cpp
    lib/Status/Status.cpp
    lib/Wifi/Wifi.cpp
    lib/Wifi/WifiSaved.cpp
    lib/Wifi/WifiScan.cpp
    lib/Wifi/WifiWatchdog.cpp
    lib/WifiPortal/WifiPortal.cpp
    lib/StaticAssets/StaticAssets.cpp
    lib/PersistBlob/PersistBlob.cpp
    lib/Settings/Settings.cpp
    lib/Settings/SettingsFields.cpp
    lib/RunSettings/RunSettings.cpp
    lib/Machine/Machine.cpp
    lib/Watchdog/Watchdog.cpp
    lib/Mode/Mode.cpp
    lib/Mode/ModeParams.cpp
    lib/Mode/ModeParamCodec.cpp
    lib/Mode/ModeMath.cpp
    lib/Mode/ModeMotion.cpp
    lib/Mode/ModeGear.cpp
    lib/Mode/ModeAsync.cpp
    lib/Mode/ModePass.cpp
    lib/Mode/ModePassPlan.cpp
    lib/Mode/ModeTurn.cpp
    lib/Mode/ModeThread.cpp
    lib/Mode/ModePart.cpp
    lib/Mode/ModeSlot.cpp
    lib/Mode/ModeEllipse.cpp
    lib/Mode/ModeGcode.cpp
    lib/Mode/ModeProbe.cpp
    lib/Mode/ProbeCycle.cpp
    lib/Probe/ProbeInput.cpp
    lib/Pins/Pins.cpp
    lib/Pins/PinMap.cpp
    lib/Pins/PinMapJson.cpp
    lib/Thread/ThreadTable.cpp
    lib/Thread/ThreadStore.cpp
    lib/Thread/ThreadStoreJson.cpp
    lib/Thread/ThreadTableJson.cpp
    lib/Backup/Backup.cpp
    lib/Backup/JsonSpan.cpp
    lib/Encoder/Encoder.cpp
    lib/Encoder/EncoderFilter.cpp
    lib/Handwheel/Handwheel.cpp
    lib/Stepper/Stepper.cpp
    lib/Stepper/StepperRamp.cpp
    lib/Stepper/StepperLimits.cpp
    lib/Sync/Sync.cpp
    lib/Sync/SyncRatio.cpp
    lib/Jog/Jog.cpp
    lib/Jog/JogRatio.cpp
    lib/Switches/Switches.cpp
    lib/Rpm/Rpm.cpp
    lib/Rpm/RpmCalc.cpp
    lib/Gcode/GcodeCore.cpp
    lib/Gcode/GcodeLathecode.cpp
    lib/Gcode/GcodeFlavors.cpp
    lib/GcodeStore/GcodeMeta.cpp
    lib/GcodeStore/GcodeFiles.cpp
    lib/GcodeStore/GcodeStore.cpp
    lib/WebUi/WebUi.cpp
    lib/WebUi/WebHttp.cpp
    lib/WebUi/WebText.cpp
    lib/WebUi/WebUpdate.cpp
    lib/WebUi/WebDocs.cpp
    lib/WebUi/WebGcode.cpp
    lib/WebUi/WsCommand.cpp
    lib/WebUi/WsMessages.cpp
    lib/Ota/Ota.cpp
    lib/LogRing/LogRing.cpp
    lib/Events/Events.cpp
    lib/Outputs/Outputs.cpp
)

target_include_directories(app PRIVATE
    src
    lib/Command
    lib/Motion
    lib/Status
    lib/Console
    lib/PersistBlob
    lib/Settings
    lib/RunSettings
    lib/Machine
    lib/Watchdog
    lib/Mode
    lib/Probe
    lib/Pins
    include
    lib/Thread
    lib/Backup
    lib/Encoder
    lib/Handwheel
    lib/Stepper
    lib/Sync
    lib/Jog
    lib/Switches
    lib/Rpm
    lib/Gcode
    lib/GcodeStore
    lib/StaticAssets
    lib/WebUi
    lib/Ota
    lib/Wifi
    lib/WifiPortal
    lib/LogRing
    lib/Events
    lib/Outputs
    ${CMAKE_BINARY_DIR}/include
    # WsCommand reads a client's peer address from Zephyr's internal websocket context.
    ${ZEPHYR_BASE}/subsys/net/lib/websocket
)

zephyr_get(SCREWSYNC_TARGET SYSBUILD LOCAL)
if(NOT SCREWSYNC_TARGET)
    message(FATAL_ERROR "SCREWSYNC_TARGET is not set -- build through the Makefile")
endif()
target_compile_definitions(app PRIVATE SCREWSYNC_TARGET="${SCREWSYNC_TARGET}")
zephyr_get(SCREWSYNC_DISPLAY SYSBUILD LOCAL)
if(NOT SCREWSYNC_DISPLAY)
    message(FATAL_ERROR "SCREWSYNC_DISPLAY is not set -- build through the Makefile (TARGET=...)")
endif()
set(SCREWSYNC_MAIN_DIR ${CMAKE_CURRENT_LIST_DIR})
include(${CMAKE_CURRENT_LIST_DIR}/../displays/${SCREWSYNC_DISPLAY}/display.cmake)

target_compile_options(app PRIVATE -Werror)

zephyr_linker_sources(SECTIONS sections-rom.ld)

execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE FW_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GIT_DESCRIBE_RESULT
)
if(NOT GIT_DESCRIBE_RESULT EQUAL 0)
    set(FW_VERSION "dev")
endif()
target_compile_definitions(app PRIVATE FW_VERSION="${FW_VERSION}")

set(WEB_BUNDLE ${CMAKE_CURRENT_LIST_DIR}/../.build/firmware-client/index.html)
if(NOT EXISTS ${WEB_BUNDLE})
    message(FATAL_ERROR "${WEB_BUNDLE} is missing -- run `make web-build` first (builds it in Docker, no Node on host)")
endif()
generate_inc_file_for_target(app ${WEB_BUNDLE} ${CMAKE_BINARY_DIR}/include/web_index_html_gz.inc --gzip)
