﻿FILE(GLOB_RECURSE BUILD_FILES CONFIGURE_DEPENDS 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

ADD_LIBRARY(devue_official_plugin SHARED
    ${PLUGIN_CORE_HPP}
    ${PLUGIN_CORE_CPP}
    ${PLUGIN_RC}
    ${BUILD_FILES}
)

SET_TARGET_PROPERTIES(devue_official_plugin PROPERTIES OUTPUT_NAME "dv_plg_devue_official")

TARGET_INCLUDE_DIRECTORIES(devue_official_plugin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
TARGET_INCLUDE_DIRECTORIES(devue_official_plugin PRIVATE "${PLUGIN_LIBRARIES_DIR}")
TARGET_INCLUDE_DIRECTORIES(devue_official_plugin PRIVATE "${PLUGIN_LIBRARIES_DIR}/glm")
TARGET_INCLUDE_DIRECTORIES(devue_official_plugin PRIVATE "${PLUGIN_LIBRARIES_DIR}/json")
TARGET_INCLUDE_DIRECTORIES(devue_official_plugin PRIVATE "${DEVUE_PLUGIN_INCLUDES}")
TARGET_INCLUDE_DIRECTORIES(devue_official_plugin PRIVATE "${DEVUE_PLUGIN_CORE}")
