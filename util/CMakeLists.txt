add_library(util STATIC
  file.c list.c program.c util.c value.c mygetopt.c seqmap.c path.c cp932.c
  file.h list.h program.h util.h value.h mygetopt.h seqmap.h path.h cp932.h
  cp932tab.h
)
target_link_libraries(util PRIVATE thtk_warning)
