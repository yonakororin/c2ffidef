cmake_minimum_required(VERSION 3.16)
project(c2ffidef CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Locate libclang ──────────────────────────────────────────────────────────
# Try pkg-config first, then fall back to well-known LLVM paths.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBCLANG QUIET libclang)
endif()

if(NOT LIBCLANG_FOUND)
    # Common LLVM versioned install paths (Debian/Ubuntu)
    foreach(LLVM_VER 18 17 16 15)
        set(_llvm_dir "/usr/lib/llvm-${LLVM_VER}")
        if(EXISTS "${_llvm_dir}/lib/libclang.so")
            set(LIBCLANG_INCLUDE_DIRS "${_llvm_dir}/include")
            set(LIBCLANG_LIBRARIES    "${_llvm_dir}/lib/libclang.so")
            set(LIBCLANG_FOUND TRUE)
            message(STATUS "Found libclang-${LLVM_VER} at ${_llvm_dir}")
            break()
        endif()
    endforeach()
endif()

if(NOT LIBCLANG_FOUND)
    message(FATAL_ERROR
        "libclang not found. Install it with:\n"
        "  sudo apt install libclang-18-dev\n"
        "or set LIBCLANG_INCLUDE_DIRS / LIBCLANG_LIBRARIES manually.")
endif()

# ── Sources ──────────────────────────────────────────────────────────────────
add_executable(c2ffidef
    src/main.cpp
    src/parser.cpp
    src/php_ffi_gen.cpp
    src/ctypes_gen.cpp
    src/validator.cpp
)

target_include_directories(c2ffidef PRIVATE
    src
    ${LIBCLANG_INCLUDE_DIRS}
)

target_link_libraries(c2ffidef PRIVATE ${LIBCLANG_LIBRARIES})

target_compile_options(c2ffidef PRIVATE -Wall -Wextra -Wpedantic)

# ── Install ──────────────────────────────────────────────────────────────────
install(TARGETS c2ffidef DESTINATION bin)
