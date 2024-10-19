include_directories(${CMAKE_SOURCE_DIR})
add_library(thtk
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

  match.c

  util.h thtk.h)
target_link_libraries(thtk PRIVATE thtk_warning $<$<BOOL:${OPENMP_FOUND}>:OpenMP::OpenMP_C>)
set_target_properties(thtk PROPERTIES
  PUBLIC_HEADER "thtk.h;error.h;io.h;dat.h;detect.h;thcrypt.h;thlzss.h"
  VERSION "1.0.0"
  SOVERSION 1
  C_VISIBILITY_PRESET hidden)

find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
  configure_file(thtk.pc.in ${CMAKE_CURRENT_BINARY_DIR}/thtk.pc)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/thtk.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()
install(TARGETS thtk
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/thtk)
