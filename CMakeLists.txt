cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(ring-zkFLIOP VERSION 0.1)

option(ENABLE_PROTOCOL_OMP "Enables Open-MP for the protocols. Disable this if you want no parallelization (better optimization than with threads=1) and/or 256 bit support (incompatible with OMP)" ON)
option(VERIFY_MOD_2_256 "Runs the verification over 256 bit integers, this only works on x86 with some compiler versions" OFF)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_C_FLAGS "-march=native -maes -mrdseed -g")
set(CMAKE_C_FLAGS "-march=native -g")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")

# Path to custom cmake files for external libs
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(THREADS_PREFER_PTHREAD_FLAG ON)

# Find external dependencies
find_package(GMP REQUIRED)
# assume built-in pthreads on MacOS
IF(APPLE)
    set(CMAKE_THREAD_LIBS_INIT "-lpthread")
    set(CMAKE_HAVE_THREADS_LIBRARY 1)
    set(CMAKE_USE_WIN32_THREADS_INIT 0)
    set(CMAKE_USE_PTHREADS_INIT 1)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
ELSE()
find_package(Threads REQUIRED)
ENDIF()
find_package(Boost 1.72.0 COMPONENTS program_options REQUIRED)
find_package(emp-tool REQUIRED)
find_package(nlohmann_json 3.9.0 REQUIRED)

if(EMP-TOOL_FOUND)
    add_library(EMPTool INTERFACE)
    target_link_libraries(EMPTool INTERFACE ${EMP-TOOL_LIBRARIES})
    target_include_directories(EMPTool INTERFACE ${EMP-TOOL_INCLUDE_DIRS})
endif()

find_package(OpenMP REQUIRED)

add_subdirectory(src)
add_subdirectory(benchmark)
