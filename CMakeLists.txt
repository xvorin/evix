cmake_minimum_required(VERSION 3.10)
project(evix VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Emit build/compile_commands.json so editors/clangd resolve the "evix/" include
# prefix via the real build flags (symlink the file to the repo root to use it).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to an optimized build when the user did not pick one explicitly.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

option(EVIX_BUILD_TESTS "Build evix unit/integration tests" OFF)
option(EVIX_SANITIZE "Enable ASan+UBSan for evix and its tests" OFF)
set(EVIX_SANITIZE_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)

# Source files - listed explicitly so adding/removing a file forces a
# reconfigure (file(GLOB) does not and silently misses new files).
set(EVIX_SOURCES
    base/base64.cpp
    base/current_thread.cpp
    base/logging.cpp
    base/process_info.cpp
    base/string_util.cpp
    base/thread_pool.cpp
    base/timestamp.cpp
    net/acceptor.cpp
    net/buffer.cpp
    net/channel.cpp
    net/connector.cpp
    net/epoll_poller.cpp
    net/event_loop.cpp
    net/event_loop_manager.cpp
    net/event_loop_thread_pool.cpp
    net/inet_address.cpp
    net/socket.cpp
    net/sockets_ops.cpp
    net/tcp_client.cpp
    net/tcp_connection.cpp
    net/tcp_server.cpp
    net/timer.cpp
    net/timer_queue.cpp
    net/udp_transceiver.cpp
)

add_library(evix SHARED ${EVIX_SOURCES})

target_compile_options(evix PRIVATE -Wall -Wextra)

if(EVIX_SANITIZE)
    target_compile_options(evix PUBLIC ${EVIX_SANITIZE_FLAGS})
    target_link_options(evix PUBLIC ${EVIX_SANITIZE_FLAGS})
endif()

# Expose the public headers under an "evix/" prefix in the build tree, so that
# in-tree includes (<evix/base/...>, <evix/net/...>) match the installed layout
# (include/evix/base, include/evix/net) without physically moving the sources.
set(EVIX_BUILD_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${EVIX_BUILD_INCLUDE_DIR}/evix)
if(NOT EXISTS ${EVIX_BUILD_INCLUDE_DIR}/evix/base)
    file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/base ${EVIX_BUILD_INCLUDE_DIR}/evix/base SYMBOLIC)
endif()
if(NOT EXISTS ${EVIX_BUILD_INCLUDE_DIR}/evix/net)
    file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/net ${EVIX_BUILD_INCLUDE_DIR}/evix/net SYMBOLIC)
endif()

target_include_directories(evix PUBLIC
    $<BUILD_INTERFACE:${EVIX_BUILD_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(evix PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME "evix"
)

# Dependencies
# Thread support. NOTE: on glibc >= 2.34 pthread is folded into libc, so
# FindThreads decides no flag is needed and leaves Threads::Threads empty --
# THREADS_PREFER_PTHREAD_FLAG is then ignored. But our public headers
# (class_registrar.h) hard-require _REENTRANT, which only the -pthread *compile*
# flag defines. So pass -pthread explicitly, PUBLIC so every consumer that
# includes those headers gets it too.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_compile_options(evix PUBLIC -pthread)
target_link_options(evix PUBLIC -pthread)

target_link_libraries(evix
    PUBLIC
        Threads::Threads
)

if(EVIX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

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

# Install headers preserving the base/ and net/ subdirs, i.e.
# include/evix/base/*.h and include/evix/net/*.h (no trailing slash on the
# source dirs so the directories themselves are copied, not just their files).
install(DIRECTORY base net
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/evix
    FILES_MATCHING PATTERN "*.h"
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
