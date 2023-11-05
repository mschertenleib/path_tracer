cmake_minimum_required(VERSION 3.20)

project(path_tracer LANGUAGES CXX)


set(CPM_DOWNLOAD_VERSION 0.38.2)

if (CPM_SOURCE_CACHE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif (DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else ()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif ()

if (NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD
            https://github.com/TheLartians/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
            ${CPM_DOWNLOAD_LOCATION}
    )
endif ()

include(${CPM_DOWNLOAD_LOCATION})


find_package(Vulkan REQUIRED)


set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
CPMAddPackage(
        GITHUB_REPOSITORY glfw/glfw
        GIT_TAG 3.3.8
)


add_executable(path_tracer
        src/main.cpp
        src/geometry.cpp src/geometry.hpp
        src/renderer.cpp src/renderer.hpp
        src/utility.cpp src/utility.hpp
        src/stb_image.cpp
        src/stb_image_write.cpp
        src/tiny_obj_loader.cpp
        src/vk_mem_alloc.cpp
)
target_compile_features(path_tracer PRIVATE cxx_std_20)
target_compile_definitions(path_tracer PRIVATE VK_NO_PROTOTYPES)
target_include_directories(path_tracer SYSTEM PRIVATE
        external
        ${Vulkan_INCLUDE_DIRS}
)
target_link_libraries(path_tracer PRIVATE glfw)


set(SHADER_SOURCES
        src/shader.rgen
        src/shader.rmiss
        src/shader.rchit
)
foreach (SHADER_SOURCE IN LISTS SHADER_SOURCES)
    cmake_path(ABSOLUTE_PATH SHADER_SOURCE BASE_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE GLSL_SHADER)
    cmake_path(GET GLSL_SHADER FILENAME SPV_SHADER)
    cmake_path(APPEND_STRING SPV_SHADER .spv)
    cmake_path(ABSOLUTE_PATH SPV_SHADER BASE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_command(
            OUTPUT ${SPV_SHADER}
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} --target-env=vulkan1.3 -O ${GLSL_SHADER} -o ${SPV_SHADER}
            DEPENDS ${GLSL_SHADER}
            COMMENT "Compiling ${SHADER_SOURCE}"
    )
    list(APPEND SPV_SHADERS ${SPV_SHADER})
endforeach ()
add_custom_target(shaders ALL DEPENDS ${SPV_SHADERS})
add_dependencies(path_tracer shaders)


set(CLANG_WARNINGS
        -Wfatal-errors
        -Wall
        -Wextra
        -Wshadow
        -Wnon-virtual-dtor
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
)
set(GCC_WARNINGS
        ${CLANG_WARNINGS}
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
)
if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    target_compile_options(path_tracer PRIVATE ${CLANG_WARNINGS})
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(path_tracer PRIVATE ${GCC_WARNINGS})
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(path_tracer PRIVATE /W4)
else ()
    message(WARNING "No warnings set for compiler '${CMAKE_CXX_COMPILER_ID}'")
endif ()


message(CHECK_START "Checking for IPO support")
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported_result OUTPUT ipo_supported_output)
if (ipo_supported_result)
    message(CHECK_PASS "supported")
    set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION_MIN_SIZE_REL TRUE)
else ()
    message(CHECK_FAIL "not supported")
endif ()
