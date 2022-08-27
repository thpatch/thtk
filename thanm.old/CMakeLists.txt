include_directories(${PNG_INCLUDE_DIRS})
add_executable(thanm.old
  thanm.c image.c
  thanm.h image.h
)
target_link_libraries(thanm.old PRIVATE util ${PNG_LIBRARY} setargv thtk_warning)
install(TARGETS thanm.old DESTINATION bin)
install(FILES thanm.old.1 DESTINATION share/man/man1)
