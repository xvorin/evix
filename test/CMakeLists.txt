cmake_minimum_required(VERSION 3.16)

# This file works both standalone (configure test/ directly) and as a
# subdirectory of the top-level build (add_subdirectory(test)).
if(NOT TARGET evix)
    project(evix_test VERSION 1.0.0 LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    find_package(evix QUIET)
    if(NOT evix_FOUND)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_BINARY_DIR}/evix_build)
    endif()

    enable_testing()
endif()

# Threads::Threads is still linked below; the -pthread compile/link flag that
# class_registrar.h needs (_REENTRANT) is propagated transitively from the evix
# target, which carries it as a PUBLIC option (see top-level CMakeLists.txt).
find_package(Threads REQUIRED)

set(EVIX_TEST_TARGETS
    evix_test_base
    evix_test_net
    evix_test_integration
)

add_executable(evix_test_base test_base.cpp)
add_executable(evix_test_net test_net.cpp)
add_executable(evix_test_integration test_integration.cpp)

foreach(t IN LISTS EVIX_TEST_TARGETS)
    target_link_libraries(${t} PRIVATE evix Threads::Threads)
    target_include_directories(${t} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    target_compile_options(${t} PRIVATE -Wall -Wextra -g)
    add_test(NAME ${t} COMMAND ${t})
endforeach()
