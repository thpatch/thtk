add_executable(thanm.old
  thanm.c image.c
  thanm.h image.h
)
target_link_libraries(thanm.old PRIVATE util $<$<BOOL:${PNG_FOUND}>:PNG::PNG> setargv thtk_warning)
install(TARGETS thanm.old DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES thanm.old.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
