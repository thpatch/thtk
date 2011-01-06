# Sets PACK_ATTRIBUTE and PACK_PRAGMA.
dnl TODO: Check with nm instead when cross-compiling.
AC_DEFUN([THTK_CHECK_PACKING],
         [AC_DEFINE([PACK_ATTRIBUTE], [],
                    [Define to __attribute__((packed)) if supported by your C compiler.])
          AH_TEMPLATE([PACK_PRAGMA],
                      [Define to 1 if your C compiler supports the packing pragma.])
          AC_MSG_CHECKING([whether structure packing can be controlled])
          thtk_struct_packing="unsupported"
          AC_RUN_IFELSE([AC_LANG_PROGRAM([dnl
                          #include <stddef.h>
                          struct test_t { char a;
                          #pragma pack(push,1)
                          int b; };
                          #pragma pack(pop)],
                          [return offsetof(struct test_t, b) == 1 ? 0 : 1;],)],
                        [AC_DEFINE([PACK_PRAGMA], [1])
                         thtk_struct_packing="pragma pack(push,1)"],
                        [],
                        [AC_DEFINE([PACK_PRAGMA], [1])
                         thtk_struct_packing="(assumed) pragma pack(push,1)"])
          AS_IF([test "x$thtk_struct_packing" = xunsupported],
                [AC_RUN_IFELSE([AC_LANG_PROGRAM([dnl
                                 #include <stddef.h>
                                 struct test_t { char a; int b; } __attribute__((packed));],
                                 [return offsetof(struct test_t, b) == 1 ? 0 : 1;],)],
                               [AC_DEFINE([PACK_ATTRIBUTE], [__attribute__((packed))])
                                thtk_struct_packing="__attribute__((packed))"])])
          AC_MSG_RESULT([$thtk_struct_packing])
          ])
