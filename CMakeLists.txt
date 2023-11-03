cmake_minimum_required(VERSION 3.20)

project(path_tracer LANGUAGES CXX)


find_package(Vulkan REQUIRED)


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



add_executable(path_tracer_test
        test/main.cpp
        test/geometry.cpp test/geometry.hpp
        test/renderer.cpp test/renderer.hpp
        test/utility.cpp test/utility.hpp
        test/stb_image.cpp
        test/stb_image_write.cpp
        test/tiny_obj_loader.cpp
        test/vk_mem_alloc.cpp
)
target_compile_features(path_tracer_test PRIVATE cxx_std_20)
target_compile_definitions(path_tracer_test PRIVATE VK_NO_PROTOTYPES)
target_include_directories(path_tracer_test SYSTEM PRIVATE
        external
        ${Vulkan_INCLUDE_DIRS}
)
if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    target_compile_options(path_tracer_test PRIVATE ${CLANG_WARNINGS})
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(path_tracer_test PRIVATE ${GCC_WARNINGS})
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(path_tracer_test PRIVATE /W4)
else ()
    message(WARNING "No warnings set for compiler '${CMAKE_CXX_COMPILER_ID}'")
endif ()