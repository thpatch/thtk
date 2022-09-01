if (WITH_LIBPNG_SOURCE)
  # CUSTOM ZLIB-NG BUILD
  file(COPY zlib-ng/zconf.h.in zlib-ng/zconf-ng.h.in zlib-ng/zlib.h zlib-ng/zlib-ng.h
       DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/zlib-ng)
  file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/zlib-ng/zconf.h.in
              ${CMAKE_CURRENT_BINARY_DIR}/zlib-ng/zconf.h)
  file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/zlib-ng/zconf-ng.h.in
              ${CMAKE_CURRENT_BINARY_DIR}/zlib-ng/zconf-ng.h)

  add_library(z STATIC
  zlib-ng/adler32.c
  zlib-ng/adler32_p.h
  zlib-ng/chunkset.c
  zlib-ng/chunkset_tpl.h
  zlib-ng/compare258.c
  zlib-ng/compress.c
  zlib-ng/crc32.c
  zlib-ng/crc32_comb.c
  zlib-ng/crc32_comb_tbl.h
  zlib-ng/crc32_p.h
  zlib-ng/crc32_tbl.h
  zlib-ng/deflate.c
  zlib-ng/deflate.h
  zlib-ng/deflate_fast.c
  zlib-ng/deflate_medium.c
  zlib-ng/deflate_p.h
  zlib-ng/deflate_quick.c
  zlib-ng/deflate_slow.c
  zlib-ng/functable.c
  zlib-ng/functable.h
  zlib-ng/infback.c
  zlib-ng/inffast.c
  zlib-ng/inffast.h
  zlib-ng/inffixed_tbl.h
  zlib-ng/inflate.c
  zlib-ng/inflate.h
  zlib-ng/inflate_p.h
  zlib-ng/inftrees.c
  zlib-ng/inftrees.h
  zlib-ng/insert_string.c
  zlib-ng/insert_string_tpl.h
  zlib-ng/match_tpl.h
  zlib-ng/trees.c
  zlib-ng/trees.h
  zlib-ng/trees_emit.h
  zlib-ng/trees_tbl.h
  zlib-ng/uncompr.c
  zlib-ng/zbuild.h
  zlib-ng/zendian.h
  zlib-ng/zutil.c
  zlib-ng/zutil.h

  zlib-ng/zlib.h)
  target_include_directories(z PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/zlib-ng)
  if (HAVE_UNISTD_H)
    set(Z_HAVE_UNISTD_H "Z_HAVE_UNISTD_H")
  endif()
  target_compile_definitions(z PRIVATE ZLIB_COMPAT)
  target_compile_definitions(z PUBLIC ${Z_HAVE_UNISTD_H})

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
  target_link_libraries(png PRIVATE z math)
  add_library(PNG::PNG ALIAS png)
  set(PNG_FOUND 1 PARENT_SCOPE)
else()
  find_package(PNG 1.6)
  if (PNG_FOUND)
    set_target_properties(PNG::PNG PROPERTIES IMPORTED_GLOBAL ON)
    set(PNG_FOUND ${PNG_FOUND} PARENT_SCOPE)
  endif()
endif()
