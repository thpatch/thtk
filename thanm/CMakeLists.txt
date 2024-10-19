find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
bison_target(AnmParse anmparse.y ${CMAKE_CURRENT_BINARY_DIR}/anmparse.c COMPILE_FLAGS ${BISON_FLAGS})
flex_target(AnmScan anmscan.l ${CMAKE_CURRENT_BINARY_DIR}/anmscan.c)
add_flex_bison_dependency(AnmScan AnmParse)
add_executable(thanm
  ${BISON_AnmParse_OUTPUT_SOURCE} ${FLEX_AnmScan_OUTPUTS}
  thanm.c image.c anmmap.c reg.c expr.c
  thanm.h image.h anmmap.h reg.h expr.h
)
target_include_directories(thanm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(thanm PRIVATE util $<$<BOOL:${PNG_FOUND}>:PNG::PNG> math setargv thtk_warning)
install(TARGETS thanm DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES thanm.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
