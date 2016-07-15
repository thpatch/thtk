include_directories(${CMAKE_SOURCE_DIR})
add_library(thtk SHARED
  bits.c error.c io.c
  thcrypt.c thcrypt105.c rng_mt.c
  thdat.c thdat02.c thdat06.c thdat08.c thdat95.c thdat105.c
  thlzss.c thrle.c
  util.c)
find_package(OpenMP)
if (OPENMP_FOUND)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
  configure_file(thtk.pc.in ${CMAKE_CURRENT_BINARY_DIR}/thtk.pc)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/thtk.pc
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
endif()
set_property(TARGET thtk PROPERTY VERSION "1.0.0")
set_property(TARGET thtk PROPERTY SOVERSION 1)
install(TARGETS thtk DESTINATION lib)
install(FILES thtk.h error.h io.h dat.h DESTINATION include/thtk)
