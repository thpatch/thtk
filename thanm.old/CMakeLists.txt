include_directories(${PNG_INCLUDE_DIRS})
add_executable(thanm.old
  thanm.c image.c
  thanm.h image.h
)
target_compile_options(thanm.old PRIVATE ${THTK_WARNING_FLAGS})
target_link_libraries(thanm.old util ${PNG_LIBRARY} setargv)
install(TARGETS thanm.old DESTINATION bin)
install(FILES thanm.old.1 DESTINATION share/man/man1)
