find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
bison_target(EcsParse ecsparse.y ${CMAKE_CURRENT_BINARY_DIR}/ecsparse.c COMPILE_FLAGS ${BISON_FLAGS})
flex_target(EcsScan ecsscan.l ${CMAKE_CURRENT_BINARY_DIR}/ecsscan.c)
add_flex_bison_dependency(EcsScan EcsParse)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
add_executable(thecl
  ${BISON_EcsParse_OUTPUT_SOURCE} ${FLEX_EcsScan_OUTPUTS}
  expr.c thecl.c eclmap.c thecl06.c thecl10.c
  expr.h thecl.h eclmap.h)
target_link_libraries(thecl PRIVATE util math setargv thtk_warning)
install(TARGETS thecl DESTINATION bin)
install(FILES thecl.1 DESTINATION share/man/man1)
