# Tries running the C compiler with the specified CFLAGS.
# The accepted flags will be placed in the variable thtk_accepted_cflags.
AC_DEFUN([THTK_TRY_CFLAGS],
         [thtk_accepted_cflags=
          m4_foreach_w([thtk_try_cflag], [$1],
                       [AC_MSG_CHECKING([whether the C compiler accepts thtk_try_cflag])
                        thtk_save_CFLAGS="$CFLAGS"
                        CFLAGS="$CFLAGS -Werror $thtk_accepted_cflags thtk_try_cflag"
                        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
                                          [dnl This adds spaces between the results, but not before the first one.
                                           thtk_accepted_cflags="$thtk_accepted_cflags${thtk_accepted_cflags:+ }thtk_try_cflag"
                                           AC_MSG_RESULT([yes])],
                                          [AC_MSG_RESULT([no])])
                        CFLAGS="$thtk_save_CFLAGS"
                        ])])
