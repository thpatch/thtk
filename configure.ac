AC_PREREQ(2.62) dnl 2.62 for AC_OPENMP.
AC_INIT([Touhou Toolkit], [3],
        [xarnonymous@gmail.com],
        [thtk],
        [http://code.google.com/p/thtk/])
AM_INIT_AUTOMAKE([1.9 foreign])
AC_CONFIG_SRCDIR([thdat.c])
AC_CONFIG_HEADERS([config.h])

dnl PACKAGE_URL was introduced in autoconf 2.64.
m4_ifndef([AC_PACKAGE_URL],
          [AC_DEFINE([PACKAGE_URL], ["http://code.google.com/p/thtk/"],
                     [Define to the home page for this package.])])

AH_TEMPLATE([HAVE_LIBPNG],
            [Define to 1 if libpng is available.])
AH_TEMPLATE([feof_unlocked],
            [These definitions are used to provide replacements
             if the unlocked stdio functions are missing.])
AH_TEMPLATE([fileno_unlocked], [...])
AH_TEMPLATE([fread_unlocked], [...])
AH_TEMPLATE([fwrite_unlocked], [...])
AH_TEMPLATE([getc_unlocked], [...])
AH_TEMPLATE([putc_unlocked], [...])
AH_TEMPLATE([__USE_MINGW_ANSI_STDIO],
            [Define to 1 to use MinGW's internal stdio library instead of Windows'.])

dnl check for programs

AC_PROG_CC_C99
AC_PROG_LEX
AM_PROG_LEX
AC_PROG_YACC

dnl check for libraries

AC_ARG_WITH([libpng],
            [AS_HELP_STRING([--without-libpng],
                            [compile the package without image support])])
AS_IF([test "x$with_libpng" != xno],
      [PKG_CHECK_MODULES([libpng], [libpng], [AC_DEFINE([HAVE_LIBPNG], [1])])])

dnl check for header files

AC_CHECK_HEADERS([dnl
    io.h dnl mkdir
    libgen.h dnl basename
    sys/mman.h dnl
])

dnl check for types

dnl check for structures

dnl check for compiler characteristics

AC_OPENMP
AS_IF([test "x$ac_cv_prog_c_openmp" = xunsupported],
      [THTK_TRY_CFLAGS([-Wno-unknown-pragmas])
       OPENMP_CFLAGS="$thtk_accepted_cflags"])

AC_ARG_ENABLE([combine],
              [AS_HELP_STRING([--disable-combine],
                              [do not compile programs in one go])])
AS_IF([test "x$enable_combine" != xno],
      [THTK_TRY_CFLAGS([-fwhole-program -combine])
       AC_SUBST([combine_CFLAGS], [$thtk_accepted_cflags])
       AS_IF([test "x$thtk_accepted_cflags" = "x-fwhole-program -combine"],
             [enable_combine=yes],
             [enable_combine=no])])
AC_MSG_CHECKING([whether the C compiler supports combined compilation])
AM_CONDITIONAL([COMBINE], [test "x$enable_combine" != xno])
AC_MSG_RESULT([$enable_combine])

THTK_CHECK_PACKING

AC_CHECK_DECL([WIN32],
              [AC_DEFINE([__USE_MINGW_ANSI_STDIO], [1])])

dnl check for library functions

AC_CHECK_FUNCS([dnl
    _splitpath dnl
    mempcpy dnl
    mmap dnl
    munmap dnl
])

AC_CHECK_FUNC([feof_unlocked],   [], [AC_DEFINE([feof_unlocked],   [feof])])
AC_CHECK_FUNC([fileno_unlocked], [], [AC_DEFINE([fileno_unlocked], [fileno])])
AC_CHECK_FUNC([fread_unlocked],  [], [AC_DEFINE([fread_unlocked],  [fread])])
AC_CHECK_FUNC([fwrite_unlocked], [], [AC_DEFINE([fwrite_unlocked], [fwrite])])
AC_CHECK_FUNC([getc_unlocked],   [], [AC_DEFINE([getc_unlocked],   [getc])])
AC_CHECK_FUNC([putc_unlocked],   [], [AC_DEFINE([putc_unlocked],   [putc])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
