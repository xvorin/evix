cmake_minimum_required(VERSION 3.16)
project(evix VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Source files - use glob to automatically find all .cpp files
file(GLOB_RECURSE EVIX_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/base/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/net/*.cpp"
)

add_library(evix SHARED ${EVIX_SOURCES})

target_include_directories(evix PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(evix PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME "evix"
)

# Dependencies
find_package(Threads REQUIRED)

target_link_libraries(evix
    PUBLIC
        Threads::Threads
)

# Install
include(GNUInstallDirs)

install(TARGETS evix
    EXPORT evixTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT evixTargets
    FILE evixTargets.cmake
    NAMESPACE xvorin::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/evix
)

# Install headers
install(DIRECTORY base/ net/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/evix
    FILES_MATCHING PATTERN "*.h"
)

# Install cmake config
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/.clang-format
    DESTINATION ${CMAKE_INSTALL_SHAREPREFIX}/evix
)

# Generate cmake config files
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/evixConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/evixConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/evix
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/evixConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/evixConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/evixConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/evix
)
