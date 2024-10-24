add_executable(thstd
  thstd.c
  thstd.h
)
target_link_libraries(thstd PRIVATE util setargv thtk_warning)
install(TARGETS thstd)
install(FILES thstd.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
