find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
bison_target(AnmParse anmparse.y ${CMAKE_CURRENT_BINARY_DIR}/anmparse.c COMPILE_FLAGS ${BISON_FLAGS})
flex_target(AnmScan anmscan.l ${CMAKE_CURRENT_BINARY_DIR}/anmscan.c)
add_flex_bison_dependency(AnmScan AnmParse)
include_directories(${PNG_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
add_executable(thanm
  ${BISON_AnmParse_OUTPUT_SOURCE} ${FLEX_AnmScan_OUTPUTS}
  thanm.c image.c anmmap.c reg.c expr.c
  thanm.h image.h anmmap.h reg.h expr.h
)
target_compile_options(thanm PRIVATE ${THTK_WARNING_FLAGS})
target_link_libraries(thanm util ${PNG_LIBRARY} setargv)
install(TARGETS thanm DESTINATION bin)
install(FILES thanm.1 DESTINATION share/man/man1)
