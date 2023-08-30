
add_executable(path_tracer
        main.cpp
        simd.hpp
)
target_compile_features(path_tracer PRIVATE cxx_std_20)


set(CLANG_OPTIONS
        -march=native
        -ffast-math
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
