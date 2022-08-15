add_library(util STATIC
  file.c list.c program.c util.c value.c mygetopt.c seqmap.c path.c 
  file.h list.h program.h util.h value.h mygetopt.h seqmap.h path.h
)
target_compile_options(util PRIVATE ${THTK_WARNING_FLAGS})
