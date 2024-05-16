set(CMAKE_INCLUDE_CURRENT_DIR on)
ADD_DEFINITIONS(-DBOOST_TEST_DYN_LINK)

include_directories(
        ${FRAMEPROCESSOR_DIR}/include
        ${ODINDATA_INCLUDE_DIRS}
        ${ODINDATA_INCLUDE_DIR}/dpdk
        ${Boost_INCLUDE_DIRS}
        ${LOG4CXX_INCLUDE_DIRS}/..
        ${ZEROMQ_INCLUDE_DIRS}
        ${BLOSC_INCLUDE_DIR}
)

# Add library for OrcaQuest plugin
add_library(OrcaQuestPlugin SHARED
        OrcaQuestPlugin.cpp
        OrcaCaptureCore.cpp
        OrcaQuestCamera.cpp
        OrcaControlCore.cpp
        OrcaQuestCameraController.cpp
        OrcaQuestCameraStateMachine.cpp
)

find_library(DCAMAPI_LIBRARY NAMES dcamapi)


target_compile_options(OrcaQuestPlugin PRIVATE ${LIBDPDK_CFLAGS})
target_link_directories(OrcaQuestPlugin PRIVATE ${ODINDATA_LIBRARY_DIR} ${LIBDPDK_LIBRARY_DIRS})
target_link_libraries(OrcaQuestPlugin
        PRIVATE OdinDataDpdk rte_eal rte_kvargs rte_telemetry
        ${LIBDPDK_LDFLAGS} ${BLOSC_LIBRARIES} ${DCAMAPI_LIBRARY}
)

get_target_property(PLUGIN_INSTALL_RPATH OrcaQuestPlugin INSTALL_RPATH)

# set_target_properties(OrcaQuestPlugin PROPERTIES
#   INSTALL_RPATH "${PLUGIN_INSTALL_RPATH}:${LIBDPDK_LIBRARY_DIRS}"
# )

set_target_properties(OrcaQuestPlugin PROPERTIES
  INSTALL_RPATH "${PLUGIN_INSTALL_RPATH}:${LIBDPDK_LIBRARY_DIRS}:/usr/local/hamamatsu_dcam/api"
)

# SET(CMAKE_INSTALL_RPATH "${INSTALL_RPATH}:${LIBDPDK_LIBRARY_DIRS}")

install(TARGETS OrcaQuestPlugin LIBRARY DESTINATION lib)
