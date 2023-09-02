cmake_minimum_required(VERSION 3.20)

project(path_tracer LANGUAGES CXX)


add_compile_definitions(VK_NO_PROTOTYPES)


find_package(Vulkan REQUIRED)


add_library(stb_image STATIC
        src/stb_image.cpp
        external/stb_image.h
)
target_include_directories(stb_image PUBLIC external)


add_library(stb_image_write STATIC
        src/stb_image_write.cpp
        external/stb_image_write.h
)
target_include_directories(stb_image_write PUBLIC external)


add_library(tiny_obj_loader STATIC
        src/tiny_obj_loader.cpp
        external/tiny_obj_loader.h
)
target_include_directories(tiny_obj_loader PUBLIC external)


add_library(vk_mem_alloc STATIC
        src/vk_mem_alloc.cpp
        external/vk_mem_alloc.h
)
target_include_directories(vk_mem_alloc PUBLIC external)
target_link_libraries(vk_mem_alloc PRIVATE Vulkan::Vulkan)


add_executable(path_tracer
        src/main.cpp
        src/geometry.cpp src/geometry.hpp
        src/renderer.cpp src/renderer.hpp
)
target_compile_features(path_tracer PRIVATE cxx_std_20)
target_link_libraries(path_tracer PRIVATE
        Vulkan::Vulkan
        stb_image
        stb_image_write
        tiny_obj_loader
        vk_mem_alloc
)


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


set(CLANG_OPTIONS
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

set(GCC_OPTIONS
        ${CLANG_OPTIONS}
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
)

if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    target_compile_options(path_tracer PRIVATE ${CLANG_OPTIONS})
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(path_tracer PRIVATE ${GCC_OPTIONS})
else ()
    message(WARNING "No compile options set for compiler '${CMAKE_CXX_COMPILER_ID}'")
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
