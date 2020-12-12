# this is based on CheckTypeSize
get_filename_component(__check_struct_packing_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)

function(__check_struct_packing_impl what preamble var)
  if(NOT CMAKE_REQUIRED_QUIET)
    message(CHECK_START "Checking for ${what}")
  endif()
  set(src ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CheckStructPacking/${var}.c)
  set(bin ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CheckStructPacking/${var}.bin)
  configure_file(${__check_struct_packing_dir}/CheckStructPacking.c.in ${src} @ONLY)
  try_compile(${var} ${CMAKE_BINARY_DIR} ${src}
    COMPILE_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
    LINK_OPTIONS ${CMAKE_REQUIRED_LINK_OPTIONS}
    LINK_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES}
    CMAKE_FLAGS
      "-DCOMPILE_DEFINITIONS:STRING=${CMAKE_REQUIRED_FLAGS}"
      "-DINCLUDE_DIRECTORIES:STRING=${CMAKE_REQUIRED_INCLUDES}"
    OUTPUT_VARIABLE output
    COPY_FILE ${bin})
  if(${var})
    file(STRINGS ${bin} strings LIMIT_COUNT 1 REGEX "INFO:check")
    if("${strings}" MATCHES ".*INFO:check\\[([01])\\].*")
      set(${var} "${CMAKE_MATCH_1}")
    else()
      set(${var} "")
    endif()
  endif()
  if(${var})
    if(NOT CMAKE_REQUIRED_QUIET)
      message(CHECK_PASS "done")
    endif()
    file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
      "CheckStructPacking (${what}) success:\n${output}\n\n")
  else()
    if(NOT CMAKE_REQUIRED_QUIET)
      message(CHECK_FAIL "failed")
    endif()
    file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
      "CheckStructPacking (${what}) fail:\n${output}\n\n")
  endif()
  set(${var} ${${var}} CACHE INTERNAL "CheckStructPacking: ${what}")
endfunction()

macro(check_packing_pragma_push)
  if(NOT DEFINED HAVE_PACKING_PRAGMA_PUSH)
    __check_struct_packing_impl("#pragma pack(push,1)" "
#include <stddef.h>
#pragma pack(push,1)
struct s { char a; int b; };
#pragma pack(pop)
#define EXPR offsetof(struct s, b) == 1
" HAVE_PACKING_PRAGMA_PUSH)
  endif()
endmacro()

macro(check_packing_gnuc_attribute)
  if(NOT DEFINED HAVE_PACKING_GNUC_ATTRIBUTE)
    __check_struct_packing_impl("__attribute__((packed))" "
#include <stddef.h>
struct __attribute__((__packed__)) s { char a; int b; };
#define EXPR offsetof(struct s, b) == 1
" HAVE_PACKING_GNUC_ATTRIBUTE)
  endif()
endmacro()

