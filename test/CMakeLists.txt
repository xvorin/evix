cmake_minimum_required(VERSION 3.16)

project(evix_test VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Force pthread for all targets
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

# Find or build evix
find_package(evix QUIET)
if(NOT evix_FOUND)
    # Build evix if not found
    add_subdirectory(${CMAKE_SOURCE_DIR}/.. ${CMAKE_BINARY_DIR}/evix_build)
endif()

# Find Threads
find_package(Threads REQUIRED)

# Base tests executable
add_executable(evix_test_base
    test_base.cpp
)

target_link_libraries(evix_test_base
    PRIVATE
        evix
        Threads::Threads
)

target_include_directories(evix_test_base
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/..
)

# Net tests executable
add_executable(evix_test_net
    test_net.cpp
)

target_link_libraries(evix_test_net
    PRIVATE
        evix
        Threads::Threads
)

target_include_directories(evix_test_net
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/..
)

# Enable all warnings for tests
target_compile_options(evix_test_base PRIVATE -Wall -Wextra -g -O0)
target_compile_options(evix_test_net PRIVATE -Wall -Wextra -g -O0)
