add_executable(thmsg
  thmsg.c thmsg06.c thmsg95.c
  thmsg.h
)
target_compile_options(thmsg PRIVATE ${THTK_WARNING_FLAGS})
target_link_libraries(thmsg util)
link_setargv(thmsg)
install(TARGETS thmsg DESTINATION bin)
install(FILES thmsg.1 DESTINATION share/man/man1)
