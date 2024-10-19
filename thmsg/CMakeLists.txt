add_executable(thmsg
  thmsg.c thmsg06.c thmsg95.c
  thmsg.h
)
target_link_libraries(thmsg PRIVATE util setargv thtk_warning)
install(TARGETS thmsg DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES thmsg.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
