# CompilerOptions.cmake — centralized compiler configuration.
#
# veltrix_configure_target(<target>) applies the project-wide C++20 standard
# and warning policy to a target. Keep flags centralized here so Windows
# (MSVC) and Linux (GCC/Clang) stay consistent.

function(veltrix_configure_target _target)
    set_property(TARGET ${_target} PROPERTY CXX_STANDARD 20)
    set_property(TARGET ${_target} PROPERTY CXX_STANDARD_REQUIRED ON)
    set_property(TARGET ${_target} PROPERTY CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_definitions(${_target} PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${_target} PRIVATE
            /W4 /permissive- /utf-8 /EHsc /Zc:preprocessor)
        if(VELTRIX_WARNINGS_AS_ERRORS)
            target_compile_options(${_target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${_target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow)
        if(VELTRIX_WARNINGS_AS_ERRORS)
            target_compile_options(${_target} PRIVATE -Werror)
        endif()
    endif()

    get_target_property(_target_type ${_target} TYPE)
    if((_target_type STREQUAL "STATIC_LIBRARY" OR _target_type STREQUAL "EXECUTABLE")
       AND VELTRIX_ENABLE_LTO
       AND CMAKE_BUILD_TYPE STREQUAL "Release")
        set_property(TARGET ${_target} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endfunction()
