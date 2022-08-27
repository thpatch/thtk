add_executable(thstd
  thstd.c
  thstd.h
)
target_link_libraries(thstd PRIVATE util setargv thtk_warning)
install(TARGETS thstd DESTINATION bin)
install(FILES thstd.1 DESTINATION share/man/man1)
