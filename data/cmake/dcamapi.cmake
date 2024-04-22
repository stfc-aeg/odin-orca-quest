message ("\nLooking for DCAMAPI headers and libraries")

find_path(DCAM_API_INCLUDE_PATH
    NAMES
    dcamapi4.h
    dcamprop.h
    PATHS
    /usr/local/hamamatsu_dcam/sdk/inc/
)

find_library(DCAMAPI_LIBRARY NAMES dcamapi)


include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LIBDPDK
    DEFAULT_MSG
    LIBRTE_EAL_LIBRARY
    LIBDPDK_INCLUDE_DIR
)

if(LIBDPDK_FOUND)
    set(LIBDPDK_VERSION ${PC_LIBDPDK_VERSION})
    get_filename_component(LIBDPDK_LIBRARY_DIR ${LIBRTE_EAL_LIBRARY} PATH)
    set(LIBDPDK_LIBRARY_DIRS ${LIBDPDK_LIBRARY_DIR})
    set(LIBDPDK_INCLUDE_DIRS ${LIBDPDK_INCLUDE_DIR})
    set(LIBDPDK_CFLAGS ${PC_LIBDPDK_CFLAGS})
    set(LIBDPDK_LDFLAGS ${PC_LIBDPDK_LDFLAGS})

    mark_as_advanced(
        LIBDPDK_VERSION
        LIBDPDK_INCLUDE_DIR
        LIBDPDK_INCLUDE_DIRS
        LIBDPDK_LIBRARY_DIR
        LIBDPDK_LIBRARY_DIRS
        LIBDPDK_CFLAGS
        LIBDPDK_LDFLAGS
    )

    message(STATUS "Found DPDK version: ${LIBDPDK_VERSION}")
    message(STATUS "DPDK library path: ${LIBDPDK_LIBRARY_DIR}")
    message(STATUS "DPDK include path: ${LIBDPDK_INCLUDE_DIR}")
    message(STATUS "DPDK CFLAGS: ${LIBDPDK_CFLAGS}")
    message(STATUS "DPDK LDFLAGS: ${LIBDPDK_LDFLAGS}")
else()
    message(STATUS "DPDK not found, not building DPDK support libraries")
endif()