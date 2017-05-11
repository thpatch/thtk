if (WITH_LIBPNG_SOURCE)
  # CUSTOM ZLIB-NG BUILD
  file(COPY zlib-ng/zconf.h.in zlib-ng/zlib.h
       DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/zlib-ng)
  file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/zlib-ng/zconf.h.in
              ${CMAKE_CURRENT_BINARY_DIR}/zlib-ng/zconf.h)
  include_directories(${CMAKE_CURRENT_BINARY_DIR}/zlib-ng)
  file(GLOB ZLIB_SRC "zlib-ng/*.c" zlib-ng/zlib.h)
  add_library(z STATIC ${ZLIB_SRC})
  if (HAVE_UNISTD_H)
    set(Z_HAVE_UNISTD_H "Z_HAVE_UNISTD_H")
  endif()
  target_compile_definitions(z
    PUBLIC WITH_GZFILEOP
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
  find_package(PNG)
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
