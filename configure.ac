AC_PREREQ(2.62) dnl 2.62 for AC_OPENMP.
AC_INIT([Touhou Toolkit], [1], [xarnonymous@gmail.com], [thtk], [http://code.google.com/p/thtk/])
AM_INIT_AUTOMAKE([1.11 silent-rules foreign])
AM_SILENT_RULES dnl requires automake 1.11
AC_CONFIG_SRCDIR([thdat.c])
AC_CONFIG_HEADERS([config.h])

AC_DEFINE([PACKAGE_THANM_VERSION], ["1"], [Define to the version of this package.])
AC_DEFINE([PACKAGE_THDAT_VERSION], ["7"], [Define to the version of this package.])
AC_DEFINE([PACKAGE_THECL_VERSION], ["2"], [Define to the version of this package.])

AH_TEMPLATE([HAVE_LIBPNG], [Define to 1 if libpng is available.])
AH_TEMPLATE([PACK_PRAGMA], [Define to 1 if your C compiler supports the packing pragma.])
AH_TEMPLATE([getc_unlocked], [])
AH_TEMPLATE([putc_unlocked], [])
AH_TEMPLATE([fread_unlocked], [])
AH_TEMPLATE([fwrite_unlocked], [])
AH_TEMPLATE([feof_unlocked], [])
AC_DEFINE([PACK_ATTRIBUTE], [], [Define to __attribute__((packed)) if supported by your C compiler.])

dnl check for programs

dnl AC_PROG_CC
AC_PROG_LEX
AM_PROG_LEX
AC_PROG_YACC

dnl check for libraries

PKG_CHECK_MODULES([libpng], [libpng], [AC_DEFINE([HAVE_LIBPNG], [1])])

dnl check for header files

AC_CHECK_HEADERS([iconv.h io.h libgen.h sys/stat.h windows.h])

dnl check for types

AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_INT32_T

dnl check for structures

dnl check for compiler characteristics

AC_C_BIGENDIAN
AC_C_INLINE
AC_OPENMP

AC_CHECK_DECL([WIN32],
              [AC_SUBST([mb_LIBS], [-lkernel32])])

AC_MSG_CHECKING([whether CC supports -fwhole-program -combine])
enable_combine="no"
ac_save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS -fwhole-program -combine"
AC_COMPILE_IFELSE(AC_LANG_PROGRAM([], []),
                  [AC_SUBST([combine_CFLAGS],
                            ["-fwhole-program -combine"])
                   enable_combine="yes"])
CFLAGS=$ac_save_CFLAGS
AM_CONDITIONAL([COMBINE], [test "$enable_combine" = "yes"])
AC_MSG_RESULT([$enable_combine])

dnl XXX: Maybe offsetof isn't all that portable.
AC_MSG_CHECKING([whether structure packing can be controlled])
structpacking="unsupported"
AC_RUN_IFELSE(AC_LANG_PROGRAM([dnl
                #include <stddef.h>
                struct test_t { char a;
                #pragma pack(push,1)
                int b; };
                #pragma pack(pop)],
                [return offsetof(struct test_t, b) == 1 ? 0 : 1;],),
              [AC_DEFINE([PACK_PRAGMA], [1])
               structpacking="pragma pack(push,1)"],
              [],
              [AC_DEFINE([PACK_PRAGMA], [1])
               structpacking="(assumed) pragma pack(push,1)"])
if test "$structpacking" = "unsupported"; then
AC_RUN_IFELSE(AC_LANG_PROGRAM([dnl
                #include <stddef.h>
                struct test_t { char a; int b; } __attribute__((packed));],
                [return offsetof(struct test_t, b) == 1 ? 0 : 1;],),
              [AC_DEFINE([PACK_ATTRIBUTE], [__attribute__((packed))])
               structpacking="__attribute__((packed))"])
fi
AC_MSG_RESULT([$structpacking])

dnl check for library functions

AC_CHECK_FUNCS([basename iconv_open mempcpy mkdir snprintf _snprintf _splitpath strdup strtof])

AC_CHECK_FUNC([getc_unlocked], [], [AC_DEFINE([getc_unlocked], [getc])])
AC_CHECK_FUNC([putc_unlocked], [], [AC_DEFINE([putc_unlocked], [putc])])
AC_CHECK_FUNC([fread_unlocked], [], [AC_DEFINE([fread_unlocked], [fread])])
AC_CHECK_FUNC([fwrite_unlocked], [], [AC_DEFINE([fwrite_unlocked], [fwrite])])
AC_CHECK_FUNC([feof_unlocked], [], [AC_DEFINE([feof_unlocked], [feof])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
