cmake_minimum_required(VERSION 3.6)
include(ExternalProject)

set(USE_ROCM TRUE)

if(NOT TRITON_LLVM_BUILD_DIR)
    set(TRITON_LLVM_BUILD_DIR ${CMAKE_BINARY_DIR})
endif()


project(triton)
include(CTest)
if (USE_ROCM)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${PROJECT_SOURCE_DIR}/third_party/hipify-torch/cmake")
    # include(Hipify)
else()
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
endif()

# Options
option(BUILD_TUTORIALS "Build C++ Triton tutorials" ON)
option(BUILD_PYTHON_MODULE "Build Python Triton bindings" OFF)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Default build type: Release")
  set(CMAKE_BUILD_TYPE "Release")
endif()

# Compiler flags
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
if (USE_ROCM)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__STDC_FORMAT_MACROS -fvisibility=default -std=gnu++17 -Wno-unused-result")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__STDC_FORMAT_MACROS -fvisibility=default -std=gnu++17")
endif()




##########
# LLVM
##########
get_cmake_property(_variableNames VARIABLES)
set(__variableNames ${_variableNames})

configure_file(cmake/DownloadLLVM.in ${TRITON_LLVM_BUILD_DIR}/llvm-download/CMakeLists.txt)
execute_process(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
    WORKING_DIRECTORY "${TRITON_LLVM_BUILD_DIR}/llvm-download"
)
execute_process(COMMAND "${CMAKE_COMMAND}" --build .
    WORKING_DIRECTORY "${TRITON_LLVM_BUILD_DIR}/llvm-download"
)

if (USE_ROCM)
    set(LLVM_TARGETS_TO_BUILD "AMDGPU" CACHE INTERNAL "")
else()
    set(LLVM_TARGETS_TO_BUILD "NVPTX" CACHE INTERNAL "")
endif()
set(LLVM_BUILD_RUNTIME "OFF" CACHE INTERNAL "")
set(LLVM_BUILD_RUNTIMES "OFF" CACHE INTERNAL "")
set(LLVM_BUILD_TOOLS "OFF" CACHE INTERNAL "")
set(LLVM_BUILD_UTILS "OFF" CACHE INTERNAL "")
set(LLVM_INCLUDE_BENCHMARKS "OFF" CACHE INTERNAL "")
set(LLVM_INCLUDE_DOCS "OFF" CACHE INTERNAL "")
set(LLVM_INCLUDE_EXAMPLES "OFF" CACHE INTERNAL "")
set(LLVM_INCLUDE_GO_TESTS "OFF" CACHE INTERNAL "")
set(LLVM_INCLUDE_RUNTIME "OFF" CACHE INTERNAL "")
set(LLVM_INCLUDE_TESTS "OFF" CACHE INTERNAL "")
set(LLVM_INCLUDE_TOOLS "OFF" CACHE INTERNAL "")
set(LLVM_INCLUDE_UTILS "OFF" CACHE INTERNAL "")
add_subdirectory(${TRITON_LLVM_BUILD_DIR}/llvm-src
                 ${TRITON_LLVM_BUILD_DIR}/llvm-build)
set(LLVM_ENABLE_PROJECTS "lld" CACHE INTERNAL "")
get_property(LLVM_LIBRARIES GLOBAL PROPERTY LLVM_COMPONENT_LIBS)
# remove LLVM-specific variables so we don't pollute GUI
get_cmake_property(_variableNames VARIABLES)
list(REMOVE_ITEM _variableNames ${__variableNames})
list(REMOVE_ITEM _variableNames ${LLVM_LIBRARIES})
foreach (_variableName ${_variableNames})
    unset(${_variableName} CACHE)
endforeach()
include_directories("${TRITON_LLVM_BUILD_DIR}/llvm-build/include/"
                    "${TRITON_LLVM_BUILD_DIR}/llvm-src/include/")

# Python module
if(BUILD_PYTHON_MODULE)
    message(STATUS "Adding Python module")
    # Build CUTLASS python wrapper if requested
    set(PYTHON_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/python/src)
    set(CUTLASS_INCLUDE_DIR "$ENV{CUTLASS_INCLUDE_DIR}")
    set(CUTLASS_LIBRARY_DIR "$ENV{CUTLASS_LIBRARY_DIR}")
    if(NOT("${CUTLASS_INCLUDE_DIR}" STREQUAL "") AND NOT("${CUTLASS_LIBRARY_DIR}" STREQUAL ""))
    if (USE_ROCM)
        set(CUTLASS_SRC ${PYTHON_SRC_PATH}/cutlass_hip.cc)
    else()
        set(CUTLASS_SRC ${PYTHON_SRC_PATH}/cutlass.cc)
    endif()
        add_definitions(-DWITH_CUTLASS_BINDINGS)
        set(CUTLASS_LIBRARIES "cutlass.a")
    endif()
    message(STATUS ${CUTLASS_INCLUDE_PATH})
    include_directories("." ${PYTHON_SRC_PATH} ${PYTHON_INCLUDE_DIRS} ${CUTLASS_INCLUDE_DIR})
    link_directories(${PYTHON_LINK_DIRS} ${CUTLASS_LIBRARY_DIR})
    if (USE_ROCM)
        add_definitions(-D__HIP_PLATFORM_AMD__)
        set(PYTHON_SRC ${PYTHON_SRC_PATH}/main.cc ${PYTHON_SRC_PATH}/triton_hip.cc  ${PYTHON_SRC_PATH}/superblock_hip.cc ${CUTLASS_SRC})
    else()
        set(PYTHON_SRC ${PYTHON_SRC_PATH}/main.cc ${PYTHON_SRC_PATH}/triton.cc  ${PYTHON_SRC_PATH}/superblock.cc ${CUTLASS_SRC})
    endif()
endif()

# Triton
file(GLOB_RECURSE LIBTRITON_SRC lib/*.cc)

if (USE_ROCM)
    # collect hip src files
    file(GLOB_RECURSE LIBTRITON_HIP_SRC *_hip.*)
    list(FILTER LIBTRITON_HIP_SRC EXCLUDE REGEX ".*triton/python/build.*$") # don't include file from build

    # get CUDA analog of HIP srcs
    set(LIBTRITON_CUDA_ANALOG_SRC ${LIBTRITON_CUDA_ANALOG_SRC})
    foreach(HIP_SRC IN LISTS LIBTRITON_HIP_SRC)
        string(REPLACE "_hip." "." CUDA_SRC ${HIP_SRC})
        list(APPEND LIBTRITON_CUDA_ANALOG_SRC ${CUDA_SRC})
    endforeach()

    # remove CUDA analogs of HIP srcs
    foreach(SRC IN LISTS LIBTRITON_SRC)
        if (${SRC} IN_LIST LIBTRITON_CUDA_ANALOG_SRC)
            list(REMOVE_ITEM LIBTRITON_SRC ${SRC})
        endif()
    endforeach()
else ()
    # fitler hip files
    file(GLOB_RECURSE LIBTRITON_HIP_SRC *_hip.*)
    foreach(SRC IN LISTS LIBTRITON_SRC)
        if (${SRC} IN_LIST LIBTRITON_HIP_SRC)
            list(REMOVE_ITEM LIBTRITON_SRC ${SRC})
        endif()
    endforeach()
endif()

add_library(triton SHARED ${LIBTRITON_SRC} ${PYTHON_SRC})
target_link_libraries(triton ${LLVM_LIBRARIES})

if(BUILD_PYTHON_MODULE)
    target_link_libraries(triton ${TORCH_LIBRARIES} ${CUTLASS_LIBRARIES})
endif()
