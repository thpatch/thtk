include_directories(${PNG_INCLUDE_DIRS})
add_executable(thanm
  thanm.c image.c
  thanm.h image.h
)
target_link_libraries(thanm util ${PNG_LIBRARY})
link_setargv(thanm)
install(TARGETS thanm DESTINATION bin)
install(FILES thanm.1 DESTINATION share/man/man1)
