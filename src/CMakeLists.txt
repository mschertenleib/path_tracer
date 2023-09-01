
find_package(Vulkan)
if (NOT Vulkan_FOUND)
    return()
endif ()


add_executable(vulkan_path_tracer
        main.cpp
        geometry.cpp geometry.hpp
        vulkan_renderer.cpp vulkan_renderer.hpp
)
target_include_directories(vulkan_path_tracer PRIVATE ${Vulkan_INCLUDE_DIRS})
target_compile_features(vulkan_path_tracer PRIVATE cxx_std_20)
target_compile_definitions(vulkan_path_tracer PRIVATE VK_NO_PROTOTYPES)
target_link_libraries(vulkan_path_tracer PRIVATE
        stb_image
        stb_image_write
        tiny_obj_loader
)


set(SHADERS ray_trace.rgen ray_trace.rmiss ray_trace.rchit)
foreach (SHADER IN LISTS SHADERS)
    set(SRC_SHADER ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER})
    set(SPV_SHADER ${CMAKE_CURRENT_BINARY_DIR}/${SHADER}.spv)
    add_custom_command(
            OUTPUT ${SPV_SHADER}
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} --target-env=vulkan1.3 -O ${SRC_SHADER} -o ${SPV_SHADER}
            DEPENDS ${SRC_SHADER}
            COMMENT "Compiling ${SHADER}"
    )
    list(APPEND SPV_SHADERS ${SPV_SHADER})
endforeach ()
add_custom_target(shaders ALL DEPENDS ${SPV_SHADERS})
add_dependencies(vulkan_path_tracer shaders)


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
    target_compile_options(vulkan_path_tracer PRIVATE ${CLANG_OPTIONS})
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(vulkan_path_tracer PRIVATE ${GCC_OPTIONS})
else ()
    message(WARNING "No compile options set for compiler '${CMAKE_CXX_COMPILER_ID}'")
endif ()
