dnl (c) FumaSoftware 2012
dnl
dnl Platform detection for autotools
dnl
dnl Copying and distribution of this file, with or without modification,
dnl are permitted in any medium without royalty provided the copyright
dnl notice and this notice are preserved.  This file is offered as-is,
dnl without any warranty.
dnl
dnl platform detection
AC_DEFUN([FUMA_AX_CCACHE_FLAGS],[dnl
])

AC_DEFUN([FUMA_AX_CCACHE],[dnl
#---------------------------------------------------------------
# FUMA_AX_CCACHE start
#---------------------------------------------------------------
        AC_ARG_WITH([ccache],
            [AS_HELP_STRING([--with-ccache@<:@=ARG@:>@],
                [use CompilerCache CCACHE (ARG=yes),
                from the specified location (ARG=<path>),
                or disable it (ARG=no)
                @<:@ARG=yes@:>@ ])],
            [fuma_ax_with_ccache=${withval}],
            [fuma_ax_with_ccache="yes"])

        AS_IF([test "x${fuma_ax_with_ccache}" = "xyes"],[dnl
            AC_MSG_CHECKING(["for ccache"])
            AC_CHECK_PROG([CCACHE_TOOL], [ccache],  [ccache])

            AS_IF([test "x${CCACHE_TOOL}" = "x"],
                [AC_MSG_RESULT(["ccache was not found in the search path"])],
                [
#---------------------------------------------------------------
# FUMA_AX_SET_AX_CCACHE_FLAGS start
#---------------------------------------------------------------
                AC_COMPILE_IFELSE(
                    [AC_LANG_PROGRAM(
                        [],
                        [[
#ifndef __clang__
                        not clang
#endif
                        ]])
                    ],
                    [
#---------------------------------------------------------------
# CCache with Clang
#---------------------------------------------------------------
                    CLANG_ARGS='-Qunused-arguments';
                    CC="${CCACHE_TOOL} ${CC} ${CLANG_ARGS}";
                    CXX="${CCACHE_TOOL} ${CXX} ${CLANG_ARGS}";
                    CXCPP="${CCACHE_TOOL} $${CXXCPP} ${CLANG_ARGS}";
                    ],
                    [
#---------------------------------------------------------------
# CCache without Clang
#---------------------------------------------------------------
                    CC="${CCACHE_TOOL} ${CC}";
                    CXX="${CCACHE_TOOL} ${CXX}";
                    CXXCPP="${CCACHE_TOOL} ${CXXCPP}";
                    ])
                    export CC;
                    export CXX;
                    export CXXXPP;
                    AC_DEFINE([HAVE_CCACHE],[1],[Found Compiler Cache $(CCACHE) in path])
                    AC_SUBST(CCACHE_TOOL)
                ])
        ])
#---------------------------------------------------------------
# FUMA_AX_CCACHE end
#---------------------------------------------------------------
])
