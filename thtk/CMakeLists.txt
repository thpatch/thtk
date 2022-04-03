include_directories(${CMAKE_SOURCE_DIR})
add_library(thtk SHARED
  bits.c error.c io.c
  bits.h error.h io.h

  thcrypt.c thcrypt105.c rng_mt.c
  thcrypt.h thcrypt105.h rng_mt.h

  thdat.c thdat02.c thdat06.c thdat08.c thdat95.c thdat105.c
  thdat.h dattypes.h

  thlzss.c thrle.c
  thlzss.h thrle.h

  detect.c
  detect.h

  util.h thtk.h)
target_compile_options(thtk PRIVATE ${THTK_WARNING_FLAGS})

find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
  configure_file(thtk.pc.in ${CMAKE_CURRENT_BINARY_DIR}/thtk.pc)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/thtk.pc
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
endif()
set_property(TARGET thtk PROPERTY VERSION "1.0.0")
set_property(TARGET thtk PROPERTY SOVERSION 1)
install(TARGETS thtk DESTINATION lib)
install(FILES thtk.h error.h io.h dat.h detect.h thcrypt.h thlzss.h DESTINATION include/thtk)
