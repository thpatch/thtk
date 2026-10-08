if (WITH_LIBPNG_SOURCE)
  # CUSTOM ZLIB-NG BUILD
  if (WITH_ZLIB_SOURCE)
    # Don't download code off the internet during build
    set(FETCHCONTENT_FULLY_DISCONNECTED ON)
    set(WITH_GTEST OFF)

    set(ZLIB_COMPAT ON)
    add_subdirectory(zlib-ng EXCLUDE_FROM_ALL)
    if (NOT TARGET ZLIB::ZLIB)
      add_library(ZLIB::ZLIB ALIAS zlib-ng)
    endif()
  else()
    find_package(ZLIB REQUIRED)
  endif()

  # CUSTOM LIBPNG BUILD
  file(COPY libpng/scripts/pnglibconf.h.prebuilt libpng/png.h libpng/pngconf.h
       DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/libpng)
  file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/libpng/pnglibconf.h.prebuilt
              ${CMAKE_CURRENT_BINARY_DIR}/libpng/pnglibconf.h)
  add_library(png STATIC
  libpng/png.c
  libpng/png.h

  libpng/pngerror.c libpng/pngget.c libpng/pngmem.c libpng/pngpread.c libpng/pngread.c
  libpng/pngrio.c libpng/pngrtran.c libpng/pngrutil.c libpng/pngset.c libpng/pngtrans.c
  libpng/pngwio.c libpng/pngwrite.c libpng/pngwtran.c libpng/pngwutil.c)
  target_include_directories(png PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/libpng)
  target_link_libraries(png PRIVATE ZLIB::ZLIB math)
  target_compile_definitions(png PRIVATE PNG_ARM_NEON_OPT=0;PNG_POWERPC_VSX_OPT=0;PNG_INTEL_SSE_OPT=0;PNG_MIPS_MSA_OPT=0;PNG_LOONGARCH_LSX_OPT=0)
  add_library(PNG::PNG ALIAS png)
  set(PNG_FOUND 1 PARENT_SCOPE)
else()
  find_package(PNG 1.6)
  if (PNG_FOUND)
    set_target_properties(PNG::PNG PROPERTIES IMPORTED_GLOBAL ON)
    set(PNG_FOUND ${PNG_FOUND} PARENT_SCOPE)
  endif()
endif()
