target_sources(app PRIVATE
    ${SCREWSYNC_MAIN_DIR}/lib/Display/Display.cpp
    ${SCREWSYNC_MAIN_DIR}/lib/Display/DisplayWidgets.cpp
    ${SCREWSYNC_MAIN_DIR}/lib/Display/DisplayFormat.cpp
    ${SCREWSYNC_MAIN_DIR}/lib/Display/DisplayText.cpp
    ${SCREWSYNC_MAIN_DIR}/lib/Display/DisplayView.cpp
    ${SCREWSYNC_MAIN_DIR}/lib/Display/DisplayPanel.cpp
    ${SCREWSYNC_MAIN_DIR}/lib/Display/DisplayPicker.cpp
    ${SCREWSYNC_MAIN_DIR}/lib/Display/DisplayImage.cpp
    ${SCREWSYNC_MAIN_DIR}/lib/Display/DisplayImageStore.cpp
    ${SCREWSYNC_MAIN_DIR}/lib/Display/ModeCells.cpp
    ${SCREWSYNC_MAIN_DIR}/lib/Nextion/NextionLink.cpp
    ${SCREWSYNC_MAIN_DIR}/lib/Nextion/NextionFrame.cpp
    ${SCREWSYNC_MAIN_DIR}/lib/Nextion/NextionAttrs.cpp
    ${SCREWSYNC_MAIN_DIR}/lib/Nextion/NextionUpload.cpp
)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${SCREWSYNC_MAIN_DIR}/lib/Display
    ${SCREWSYNC_MAIN_DIR}/lib/Nextion
)
