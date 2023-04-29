cmake_minimum_required(VERSION 3.19)

project(path_tracer LANGUAGES CXX)

set(CLANG_OPTIONS
        #-march=native
        #-ffast-math
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
        -Wdouble-promotion)

set(GCC_OPTIONS
        ${CLANG_OPTIONS}
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast)

if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    set(PROJECT_OPTIONS ${CLANG_OPTIONS})
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(PROJECT_OPTIONS ${GCC_OPTIONS})
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(PROJECT_OPTIONS /W4)
else ()
    message(WARNING "No warnings set for compiler '${CMAKE_CXX_COMPILER_ID}'")
endif ()


find_package(Vulkan REQUIRED)


set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)


add_library(imgui STATIC
        external/imgui/imgui.cpp external/imgui/imgui.h
        external/imgui/imgui_internal.h
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp
        external/imgui/backends/imgui_impl_glfw.cpp
        external/imgui/backends/imgui_impl_vulkan.cpp)
target_include_directories(imgui PRIVATE
        external/imgui
        external/imgui/backends)
target_link_libraries(imgui PRIVATE
        Vulkan::Vulkan
        glfw)


add_executable(path_tracer
        src/main.cpp
        src/renderer.cpp src/renderer.hpp
        src/utils.cpp src/utils.hpp
        external/stb/stb_image.h
        external/stb/stb_image_write.h)
target_include_directories(path_tracer PRIVATE
        external/stb
        external/glfw/include
        external/imgui
        external/imgui/backends)
target_compile_options(path_tracer PRIVATE ${PROJECT_OPTIONS})
target_compile_features(path_tracer PRIVATE cxx_std_20)
target_compile_definitions(path_tracer PRIVATE
        $<$<CONFIG:Debug>:ENABLE_VALIDATION_LAYERS>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:NDEBUG>
        _CRT_SECURE_NO_WARNINGS)
target_link_libraries(path_tracer PRIVATE
        Vulkan::Vulkan
        glfw
        imgui)

set(SHADERS fullscreen.vert fullscreen.frag render.comp)
foreach (SHADER IN LISTS SHADERS)
    set(SRC_SHADER ${CMAKE_SOURCE_DIR}/src/${SHADER})
    set(SPV_SHADER ${CMAKE_BINARY_DIR}/${SHADER}.spv)
    add_custom_command(
            OUTPUT ${SPV_SHADER}
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} -O ${SRC_SHADER} -o ${SPV_SHADER}
            DEPENDS ${SRC_SHADER}
            COMMENT "Compiling ${SHADER}")
    list(APPEND SPV_SHADERS ${SPV_SHADER})
endforeach ()
add_custom_target(shaders ALL DEPENDS ${SPV_SHADERS})
add_dependencies(path_tracer shaders)


include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_supported_error)
if (ipo_supported)
    set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION_MIN_SIZE_REL TRUE)
else ()
    message(WARNING "IPO is not supported: ${ipo_supported_error}")
endif ()
