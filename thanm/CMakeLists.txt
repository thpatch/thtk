if (WITH_LIBPNG_SOURCE)
  # CUSTOM ZLIB-NG BUILD
  file(COPY zlib-ng/zconf.h.in zlib-ng/zlib.h
       DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/zlib-ng)
  file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/zlib-ng/zconf.h.in
              ${CMAKE_CURRENT_BINARY_DIR}/zlib-ng/zconf.h)
  include_directories(${CMAKE_CURRENT_BINARY_DIR}/zlib-ng)
  file(GLOB ZLIB_SRC "zlib-ng/*.c" zlib-ng/zlib.h)

  add_library(z STATIC
  zlib-ng/adler32.c
  zlib-ng/compress.c
  zlib-ng/crc32.c
  zlib-ng/crc32.h
  zlib-ng/deflate.c
  zlib-ng/deflate.h
  zlib-ng/deflate_fast.c
  zlib-ng/deflate_medium.c
  zlib-ng/deflate_p.h
  zlib-ng/deflate_slow.c
  zlib-ng/functable.c
  zlib-ng/functable.h
  zlib-ng/infback.c
  zlib-ng/inffast.c
  zlib-ng/inffast.h
  zlib-ng/inffixed.h
  zlib-ng/inflate.c
  zlib-ng/inflate.h
  zlib-ng/inftrees.c
  zlib-ng/inftrees.h
  zlib-ng/match.c
  zlib-ng/match.h
  zlib-ng/memcopy.h
  zlib-ng/trees.c
  zlib-ng/trees.h
  zlib-ng/uncompr.c
  zlib-ng/zutil.c
  zlib-ng/zutil.h

  zlib-ng/zlib.h)
  if (HAVE_UNISTD_H)
    set(Z_HAVE_UNISTD_H "Z_HAVE_UNISTD_H")
  endif()
  target_compile_definitions(z
    PUBLIC ${Z_HAVE_UNISTD_H})

  include(CheckFunctionExists)
  check_function_exists(pow BUILTIN_MATH)
  if (NOT BUILTIN_MATH)
    list(APPEND CMAKE_REQUIRED_LIBRARIES m)
    check_function_exists(pow LIBM_MATH)
    if (LIBM_MATH)
      set(LIBM m)
    else()
      message(FATAL_ERROR "No math library found")
    endif()
  endif()

  # CUSTOM LIBPNG BUILD
  file(COPY libpng/scripts/pnglibconf.h.prebuilt libpng/png.h libpng/pngconf.h
       DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/libpng)
  file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/libpng/pnglibconf.h.prebuilt
              ${CMAKE_CURRENT_BINARY_DIR}/libpng/pnglibconf.h)
  include_directories(${CMAKE_CURRENT_BINARY_DIR}/libpng)
  add_library(png STATIC
  libpng/png.c
  libpng/png.h

  libpng/pngerror.c libpng/pngget.c libpng/pngmem.c libpng/pngpread.c libpng/pngread.c
  libpng/pngrio.c libpng/pngrtran.c libpng/pngrutil.c libpng/pngset.c libpng/pngtrans.c
  libpng/pngwio.c libpng/pngwrite.c libpng/pngwtran.c libpng/pngwutil.c)
  target_link_libraries(png z ${LIBM})
  set(PNG_LIBRARY png)
else()
  find_package(PNG 1.6)
  if (PNG_FOUND)
    include_directories(${PNG_INCLUDE_DIRS})
    set(PNG_LIBRARY ${PNG_LIBRARIES})
  endif()
endif()

add_executable(thanm
  thanm.c image.c
  thanm.h image.h
)
target_link_libraries(thanm util ${PNG_LIBRARY})
link_setargv(thanm)
install(TARGETS thanm DESTINATION bin)
install(FILES thanm.1 DESTINATION share/man/man1)
