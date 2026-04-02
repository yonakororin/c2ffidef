cmake_minimum_required(VERSION 3.16)
project(c2ffidef CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Locate libclang ──────────────────────────────────────────────────────────
# Try pkg-config first, then fall back to well-known LLVM install paths.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBCLANG QUIET libclang)
endif()

if(NOT LIBCLANG_FOUND)
    # Debian / Ubuntu: /usr/lib/llvm-<ver>/lib/libclang.so
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
    # RHEL / Fedora / Oracle Linux / Rocky / AlmaLinux:
    #   - /usr/lib64/llvm<ver>/lib64/libclang.so  (versioned, lib64 subdir)
    #   - /usr/lib64/llvm<ver>/lib/libclang.so    (versioned, lib subdir)
    #   - /usr/lib64/libclang.so                  (unversioned system path)
    foreach(LLVM_VER 20 19 18 17 16 15)
        foreach(_candidate
                "/usr/lib64/llvm${LLVM_VER}/lib64/libclang.so"
                "/usr/lib64/llvm${LLVM_VER}/lib/libclang.so"
                "/usr/lib/llvm${LLVM_VER}/lib64/libclang.so"
                "/usr/lib/llvm${LLVM_VER}/lib/libclang.so")
            if(EXISTS "${_candidate}")
                get_filename_component(_llvm_base "${_candidate}" DIRECTORY)
                get_filename_component(_llvm_base "${_llvm_base}" DIRECTORY)
                # Prefer versioned include; fall back to /usr/include
                if(EXISTS "${_llvm_base}/include/clang-c/Index.h")
                    set(LIBCLANG_INCLUDE_DIRS "${_llvm_base}/include")
                else()
                    set(LIBCLANG_INCLUDE_DIRS "/usr/include")
                endif()
                set(LIBCLANG_LIBRARIES "${_candidate}")
                set(LIBCLANG_FOUND TRUE)
                message(STATUS "Found libclang-${LLVM_VER} at ${_candidate}")
                break()
            endif()
        endforeach()
        if(LIBCLANG_FOUND)
            break()
        endif()
    endforeach()
endif()

if(NOT LIBCLANG_FOUND)
    # Last resort: unversioned /usr/lib64/libclang.so (Oracle Linux / RHEL system install)
    foreach(_candidate
            "/usr/lib64/libclang.so"
            "/usr/lib/libclang.so")
        if(EXISTS "${_candidate}")
            set(LIBCLANG_INCLUDE_DIRS "/usr/include")
            set(LIBCLANG_LIBRARIES    "${_candidate}")
            set(LIBCLANG_FOUND TRUE)
            message(STATUS "Found libclang at ${_candidate}")
            break()
        endif()
    endforeach()
endif()

if(NOT LIBCLANG_FOUND)
    message(FATAL_ERROR
        "libclang not found. Install it with one of the following:\n"
        "  # Debian / Ubuntu\n"
        "  sudo apt install libclang-18-dev\n"
        "\n"
        "  # Fedora\n"
        "  sudo dnf install clang-devel\n"
        "\n"
        "  # RHEL 9 / Oracle Linux 9 / Rocky / AlmaLinux\n"
        "  sudo dnf install llvm-toolset clang-devel\n"
        "\n"
        "  # RHEL 8 系（モジュール有効化が必要）\n"
        "  sudo dnf module enable llvm-toolset\n"
        "  sudo dnf install clang-devel\n"
        "\n"
        "または LIBCLANG_INCLUDE_DIRS / LIBCLANG_LIBRARIES を手動で指定してください。")
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
