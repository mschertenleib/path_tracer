cmake_minimum_required(VERSION 3.20)

project(path_tracer LANGUAGES CXX)


include(cmake/CPM.cmake)


find_package(Vulkan REQUIRED)


set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
CPMAddPackage(
        GITHUB_REPOSITORY glfw/glfw
        GIT_TAG 3.3.8
)


CPMAddPackage(
        GITHUB_REPOSITORY ocornut/imgui
        GIT_TAG v1.89.8-docking
        DOWNLOAD_ONLY YES
)


set(BUILD_SHARED_LIBS ON)
set(ASSIMP_NO_EXPORT ON)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_WARNINGS_AS_ERRORS OFF)
set(ASSIMP_INSTALL OFF)
CPMAddPackage(
        GITHUB_REPOSITORY assimp/assimp
        GIT_TAG v5.3.1
)


set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui.h
        ${imgui_SOURCE_DIR}/imgui_internal.h
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
add_executable(path_tracer
        src/main.cpp
        src/application.cpp src/application.hpp
        src/scene.cpp src/scene.hpp
        src/renderer.cpp src/renderer.hpp
        src/utility.cpp src/utility.hpp
        src/vec3.hpp
        src/stb_image.cpp
        src/stb_image_write.cpp
        src/tiny_obj_loader.cpp
        src/vk_mem_alloc.cpp
        ${IMGUI_SOURCES}
)
target_compile_features(path_tracer PRIVATE cxx_std_20)
target_compile_definitions(path_tracer PRIVATE VK_NO_PROTOTYPES)
target_include_directories(path_tracer SYSTEM PRIVATE
        external
        ${Vulkan_INCLUDE_DIRS}
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(path_tracer PRIVATE glfw assimp)
add_custom_command(TARGET path_tracer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:assimp> ${CMAKE_CURRENT_BINARY_DIR}
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
    set_source_files_properties(${IMGUI_SOURCES} PROPERTIES COMPILE_OPTIONS
            "-Wno-conversion;-Wno-sign-conversion;-Wno-double-promotion;-Wno-null-dereference;-Wno-unused"
    )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(path_tracer PRIVATE ${GCC_WARNINGS})
    set_source_files_properties(${IMGUI_SOURCES} PROPERTIES COMPILE_OPTIONS
            "-Wno-conversion;-Wno-sign-conversion;-Wno-double-promotion;-Wno-null-dereference;-Wno-unused;-Wno-useless-cast"
    )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(path_tracer PRIVATE /W4)
    set_source_files_properties(${IMGUI_SOURCES} PROPERTIES COMPILE_OPTIONS
            "/wd4189"
    )
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
