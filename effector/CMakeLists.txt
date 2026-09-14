cmake_minimum_required(VERSION 3.16)
project(effector_native CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS lib/effector_core/src/*.cpp)
add_library(effector_core STATIC ${CORE_SOURCES})
target_include_directories(effector_core PUBLIC lib/effector_core/src)
target_compile_options(effector_core PRIVATE -Wall -Wextra -Werror)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS test/*.cpp)
add_executable(native_tests ${TEST_SOURCES})
target_include_directories(native_tests PRIVATE test)
target_link_libraries(native_tests PRIVATE effector_core)
target_compile_options(native_tests PRIVATE -Wall -Wextra -Werror)
