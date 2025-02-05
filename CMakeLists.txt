cmake_minimum_required(VERSION 3.22)

project(path_tracer LANGUAGES CXX C)


include(cmake/CPM.cmake)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


find_package(Vulkan REQUIRED)
add_compile_definitions(VK_NO_PROTOTYPES)


set(VMA_BUILD_SAMPLE OFF)
set(VMA_STATIC_VULKAN_FUNCTIONS OFF)
set(VMA_DYNAMIC_VULKAN_FUNCTIONS ON)
CPMAddPackage(
        GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
        GIT_TAG f1ab507
)


set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
CPMAddPackage(
        GITHUB_REPOSITORY glfw/glfw
        GIT_TAG 3.3.8
)


CPMAddPackage(
        GITHUB_REPOSITORY ocornut/imgui
        GIT_TAG v1.90.3-docking
        DOWNLOAD_ONLY YES
)
add_library(imgui STATIC)
target_sources(imgui PRIVATE
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui.h
        ${imgui_SOURCE_DIR}/imgui_internal.h
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui SYSTEM
        PRIVATE
        ${Vulkan_INCLUDE_DIRS}
        PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PRIVATE glfw)


CPMAddPackage(
        GITHUB_REPOSITORY CedricGuillemet/ImGuizmo
        GIT_TAG ba662b1
        DOWNLOAD_ONLY YES
)
add_library(imguizmo STATIC)
target_sources(imguizmo PRIVATE
        ${ImGuizmo_SOURCE_DIR}/ImGuizmo.cpp
        ${ImGuizmo_SOURCE_DIR}/ImGuizmo.h	
)
target_include_directories(imguizmo SYSTEM PUBLIC ${ImGuizmo_SOURCE_DIR})
target_link_libraries(imguizmo PRIVATE imgui)


CPMAddPackage(
        GITHUB_REPOSITORY nothings/stb
        GIT_TAG f4a71b1
        DOWNLOAD_ONLY YES
)


set(BUILD_SHARED_LIBS ON)
set(ASSIMP_NO_EXPORT ON)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_WARNINGS_AS_ERRORS OFF)
set(ASSIMP_INSTALL OFF)
set(ASSIMP_BUILD_ASSIMP_VIEW OFF)
# TODO: we should probably disable some importer formats that we could never load anyway
CPMAddPackage(
        GITHUB_REPOSITORY assimp/assimp
        GIT_TAG v5.3.1
)


CPMAddPackage(
        GIT_REPOSITORY "https://git.code.sf.net/p/tinyfiledialogs/code"
        NAME tinyfiledialogs
        GIT_TAG e60005
        DOWNLOAD_ONLY
)
add_library(tinyfiledialogs STATIC)
target_sources(tinyfiledialogs PRIVATE
        ${tinyfiledialogs_SOURCE_DIR}/tinyfiledialogs.c
        ${tinyfiledialogs_SOURCE_DIR}/tinyfiledialogs.h
)
target_include_directories(tinyfiledialogs PUBLIC ${tinyfiledialogs_SOURCE_DIR})


add_executable(path_tracer)
target_compile_features(path_tracer PRIVATE cxx_std_20)
target_sources(path_tracer PRIVATE
        src/main.cpp
        src/application.cpp
        src/application.hpp
        src/camera.cpp
        src/camera.hpp
        src/renderer.cpp
        src/renderer.hpp
        src/utility.cpp
        src/utility.hpp
        src/vec3.hpp
	src/vma.cpp
)
target_include_directories(path_tracer SYSTEM PRIVATE
        ${Vulkan_INCLUDE_DIRS}
        ${stb_SOURCE_DIR}
	${VulkanMemoryAllocator_SOURCE_DIR}/include
)
target_link_libraries(path_tracer PRIVATE
        glfw
        imgui
        imguizmo
        assimp
        tinyfiledialogs
)
# TODO: check that this actually works in all cases ? It seems like we might
# have to build twice for the copy to take effect in some cases
add_custom_command(TARGET path_tracer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:assimp> $<TARGET_FILE_DIR:path_tracer>
)


set(SHADER_SOURCES
        shader.rgen
        shader.rmiss
        diffuse.rchit
        specular.rchit
        emissive.rchit
)
cmake_path(APPEND CMAKE_SOURCE_DIR src shaders OUTPUT_VARIABLE GLSL_DIR)
foreach (SHADER_SOURCE IN LISTS SHADER_SOURCES)
        cmake_path(ABSOLUTE_PATH SHADER_SOURCE BASE_DIRECTORY ${GLSL_DIR} OUTPUT_VARIABLE GLSL_SHADER)
        cmake_path(GET GLSL_SHADER FILENAME SPV_SHADER)
        cmake_path(APPEND_STRING SPV_SHADER .spv)
        cmake_path(ABSOLUTE_PATH SPV_SHADER BASE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        add_custom_command(
                OUTPUT ${SPV_SHADER}
                COMMAND ${Vulkan_GLSLC_EXECUTABLE} --target-env=vulkan1.3 -O ${GLSL_SHADER} -o ${SPV_SHADER}
                COMMAND ${CMAKE_COMMAND} -E copy ${SPV_SHADER} $<TARGET_FILE_DIR:path_tracer>
                MAIN_DEPENDENCY ${GLSL_SHADER}
                VERBATIM
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


if ((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
    target_compile_options(path_tracer PRIVATE $<$<CONFIG:Debug>:-fsanitize=undefined>)
    #target_compile_options(path_tracer PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>)
    target_link_options(path_tracer PRIVATE $<$<CONFIG:Debug>:-fsanitize=undefined>)
    #target_link_options(path_tracer PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>)
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
