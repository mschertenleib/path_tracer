cmake_minimum_required(VERSION 3.19)

project(path_tracer LANGUAGES CXX)


add_subdirectory(src_cpu)
add_subdirectory(src)


add_library(stb_image STATIC
        external/stb_image.cpp
        external/stb_image.h
)
target_include_directories(stb_image PUBLIC external)


add_library(stb_image_write STATIC
        external/stb_image_write.cpp
        external/stb_image_write.h
)
target_include_directories(stb_image_write PUBLIC external)


add_library(tiny_obj_loader STATIC
        external/tiny_obj_loader.cpp
        external/tiny_obj_loader.h
)
target_include_directories(tiny_obj_loader PUBLIC external)


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
