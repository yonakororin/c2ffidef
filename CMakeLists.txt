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

# ── CPack (DEB / RPM) ────────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME        "c2ffidef")
set(CPACK_PACKAGE_VERSION     "0.1.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Generate PHP FFI / Python ctypes bindings from C headers")
set(CPACK_PACKAGE_DESCRIPTION
    "c2ffidef parses C header files using libclang and generates ready-to-use PHP FFI::cdef() and Python ctypes binding code. Resolves #define constants, structs, unions, enums, typedefs, and function declarations from single or multi-file headers.")
set(CPACK_PACKAGE_CONTACT     "https://github.com/yonakororin/c2ffidef")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/yonakororin/c2ffidef")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

# Install prefix for packaged binary
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

# ── DEB ──────────────────────────────────────────────────────────────────────
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "yonakororin")
set(CPACK_DEBIAN_PACKAGE_SECTION    "devel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY   "optional")
# Runtime dependency: the shared libclang is needed to run c2ffidef
set(CPACK_DEBIAN_PACKAGE_DEPENDS    "libclang1-18 | libclang1-17 | libclang1-16 | libclang1-15")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
# Use file name convention:  c2ffidef_<ver>_<arch>.deb
set(CPACK_DEBIAN_FILE_NAME          DEB-DEFAULT)

# ── RPM ──────────────────────────────────────────────────────────────────────
set(CPACK_RPM_PACKAGE_DESCRIPTION "c2ffidef parses C header files using libclang and generates ready-to-use PHP FFI::cdef() and Python ctypes binding code. Resolves #define constants, structs, unions, enums, typedefs, and function declarations from single or multi-file headers.")
set(CPACK_RPM_PACKAGE_LICENSE       "MIT")
set(CPACK_RPM_PACKAGE_GROUP         "Development/Tools")
set(CPACK_RPM_PACKAGE_REQUIRES      "clang-libs >= 15")
set(CPACK_RPM_PACKAGE_ARCHITECTURE  "${CMAKE_SYSTEM_PROCESSOR}")
# Use file name convention:  c2ffidef-<ver>-<release>.<arch>.rpm
set(CPACK_RPM_FILE_NAME             RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_RELEASE       "1")

# ── Generator selection ──────────────────────────────────────────────────────
# Default to building both; override with -DCPACK_GENERATOR=DEB or RPM
if(NOT CPACK_GENERATOR)
    set(CPACK_GENERATOR "DEB;RPM")
endif()

include(CPack)
