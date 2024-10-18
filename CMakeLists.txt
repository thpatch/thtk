cmake_minimum_required(VERSION 3.14)

project("Touhou Toolkit"
  VERSION 12)
set(PROJECT_URL "https://github.com/thpatch/thtk")
set(PROJECT_BUGREPORT "https://github.com/thpatch/thtk/issues")

include(cmake/CheckStructPacking.cmake)
include(CheckTypeSize)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CMakePushCheckState)
include(GenerateExportHeader)

cmake_push_check_state(RESET)

check_packing_gnuc_attribute()
if(NOT HAVE_PACKING_GNUC_ATTRIBUTE)
    check_packing_pragma_push()
    if(NOT HAVE_PACKING_PRAGMA_PUSH)
        message(SEND_ERROR "No structure packing.")
    endif()
endif()

check_include_file("sys/types.h" HAVE_SYS_TYPES_H)
if(HAVE_SYS_TYPES_H)
  set(CMAKE_EXTRA_INCLUDE_FILES "sys/types.h")
  check_type_size(off_t OFF_T BUILTIN_TYPES_ONLY)
  check_type_size(ssize_t SSIZE_T BUILTIN_TYPES_ONLY)
  cmake_reset_check_state()
endif()
if(NOT HAVE_SSIZE_T)
  set(CMAKE_EXTRA_INCLUDE_FILES "basetsd.h")
  check_type_size(SSIZE_T SSIZE_T_BASETSD BUILTIN_TYPES_ONLY)
  cmake_reset_check_state()
endif()

check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("unistd.h" HAVE_UNISTD_H)

check_symbol_exists("mmap" "sys/mman.h" HAVE_MMAP)
check_symbol_exists("scandir" "dirent.h" HAVE_SCANDIR)
check_symbol_exists("fstat" "sys/stat.h" HAVE_FSTAT)
check_symbol_exists("fileno" "stdio.h" HAVE_FILENO)
check_symbol_exists("chdir" "unistd.h" HAVE_CHDIR)
if(NOT HAVE_CHDIR)
  check_symbol_exists("_chdir" "direct.h" HAVE__CHDIR)
endif()
check_symbol_exists("pread" "unistd.h" HAVE_PREAD)

check_symbol_exists("getc_unlocked" "stdio.h" HAVE_GETC_UNLOCKED)
if(HAVE_GETC_UNLOCKED)
  check_symbol_exists("fread_unlocked" "stdio.h" HAVE_FREAD_UNLOCKED)
  if(HAVE_FREAD_UNLOCKED)
    check_symbol_exists("feof_unlocked" "stdio.h" HAVE_FEOF_UNLOCKED)
  endif()
else()
  check_symbol_exists("_getc_nolock" "stdio.h" HAVE__GETC_NOLOCK)
  if(HAVE__GETC_NOLOCK)
    check_symbol_exists("_fread_nolock" "stdio.h" HAVE__FREAD_NOLOCK)
  endif()
endif()

check_symbol_exists("pow" "math.h" HAVE_MATH_LIBC)
if(NOT HAVE_MATH_LIBC)
  set(CMAKE_REQUIRED_LIBRARIES m)
  check_symbol_exists("pow" "math.h" HAVE_MATH_LIBM)
  cmake_reset_check_state()
  if(NOT HAVE_MATH_LIBM)
    message(FATAL_ERROR "No math library found")
  endif()
endif()

cmake_pop_check_state()

add_library(math INTERFACE IMPORTED)
if(HAVE_MATH_LIBM)
  set_target_properties(math PROPERTIES INTERFACE_LINK_LIBRARIES m)
endif()

set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

option(BUILD_SHARED_LIBS "Prefer to build shared lib" ON)
option(WITH_LIBPNG_SOURCE "Compile libpng from source" ON)
option(WITH_OPENMP "Compile with OpenMP" ON)
if(UNIX)
  option(CONTRIB_UTHDAT "Build midnight commander plugin" OFF)
endif()
if(WIN32)
  option(CONTRIB_WCTHDAT "Build total commander plugin" OFF)
endif()

if(WITH_OPENMP)
  find_package(OpenMP)
endif()

if(MSVC)
  # Defining these here rather than in config.h.in also applies them to the
  # zlib-ng and libpng build.
  # Character set options were introduced in Visual Studio 2015 Update 2
  add_compile_options(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE /source-charset:utf-8 /execution-charset:utf-8)
else()
  add_compile_options(-D_GNU_SOURCE -std=c99 -finput-charset=utf-8 -fexec-charset=utf-8)
endif()

add_library(setargv INTERFACE IMPORTED)
if(MSVC)
  set_target_properties(setargv PROPERTIES INTERFACE_LINK_OPTIONS "setargv.obj")
endif()

if(MSVC)
  add_compile_options("$<$<CONFIG:Release,RelWithDebInfo>:/MT;/GL;/Ox;/Ot>;$<$<CONFIG:Debug>:/MTd>")
  add_link_options("$<$<CONFIG:Release,RelWithDebInfo>:/INCREMENTAL:NO;/OPT:REF;/LTCG>")
endif()

add_library(thtk_warning INTERFACE IMPORTED)
if(MSVC)
  set_target_properties(thtk_warning PROPERTIES
    INTERFACE_COMPILE_OPTIONS "/W3;/wd4267;/wd4244"
  )
else()
  set_target_properties(thtk_warning PROPERTIES
    INTERFACE_COMPILE_OPTIONS "-Wall;-Wextra;-Wno-parentheses;$<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-pragma-pack>;-Wno-sign-compare;-Wmissing-prototypes"
  )
endif()

set(BISON_FLAGS "" CACHE STRING "Flags used by the BISON parser generator during all build types.")

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_subdirectory(extlib)
add_subdirectory(util)
include_directories(util extlib/thtypes)
add_subdirectory(thanm)
add_subdirectory(thanm.old)
add_subdirectory(thdat)
add_subdirectory(thecl)
add_subdirectory(thmsg)
add_subdirectory(thstd)
add_subdirectory(thtk)
add_subdirectory(contrib)

configure_file(config.h.in config.h)
generate_export_header(thtk)
