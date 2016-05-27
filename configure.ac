AC_PREREQ(2.61)

AC_INIT(OOBase,0.6.0)
AC_CONFIG_SRCDIR([src/Memory.cpp])
AC_CONFIG_HEADERS([autoheader_dummy.h include/oobase-platform.h src/oobase-config.h])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES([Makefile])

AM_INIT_AUTOMAKE([foreign 1.10 no-define subdir-objects])

dnl disable unnecessary compiler checks
AC_DEFUN([AC_PROG_F77], [:])
AC_DEFUN([AC_PROG_FC], [:])
AC_DEFUN([AC_PROG_OBJC], [:])
AC_DEFUN([AC_PROG_OBJCCPP], [:])
AC_DEFUN([AC_LIBTOOL_F77], [:])

# Use the latest short output macro if available
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# I like maintainer mode
AM_MAINTAINER_MODE

AC_CANONICAL_HOST

# Set the WIN32 conditional in the makefiles if the host is MinGW
# If the host is Cygwin, error
AS_CASE([$host_os],
	[*cygwin*],[AC_MSG_ERROR([Don't build for cygwin, use a native Win32 port such as MinGW])],
	[mingw*],[win32=true]
)
AM_CONDITIONAL([WIN32],[test "x$win32" = "xtrue"])

# Add the --enable-debug arg
AC_ARG_ENABLE([debug],AS_HELP_STRING([--enable-debug],[Turn on debugging]),[debug=true],[debug=false])
AM_CONDITIONAL([DEBUG], [test "x$debug" = "xtrue"])

OO_PROG_CC
OO_PROG_CXX

# Check if we can use compiler caching
FUMA_AX_CCACHE

# Check the multi-threading flags
AS_CASE([$host_os],
	[mingw*],[PTHREAD_CFLAGS=-mthreads AC_SUBST(PTHREAD_LIBS) AC_SUBST(PTHREAD_CFLAGS)],
	[AX_PTHREAD]
)

# Set sensible default CFLAGS if necessary
AS_IF([test "x$oo_test_CFLAGS" != "xset"],
[
  AS_IF([test "x$debug" = "xtrue"],[
    AX_APPEND_COMPILE_FLAGS([-ggdb -O0 -Wall],[CFLAGS])
  ],[
    AX_APPEND_COMPILE_FLAGS([-O2],[CFLAGS])
  ])

  AS_CASE([$host_os],
    [mingw*],[AX_APPEND_COMPILE_FLAGS([-march=i686 -mtune=native],[CFLAGS])]
  )
])

# Set sensible default CXXFLAGS if necessary
AS_IF([test "x$oo_test_CXXFLAGS" != "xset"],
[
  AS_IF([test "x$debug" = "xtrue"],[
  	AX_APPEND_COMPILE_FLAGS([-ggdb -O0 -Wall],[CXXFLAGS])
  ],[
    AX_APPEND_COMPILE_FLAGS([-O2],[CXXFLAGS])
  ])

  AS_CASE([$host_os],
    [mingw*],[AX_APPEND_COMPILE_FLAGS([-march=i686 -mtune=native],[CXXFLAGS])]
  )
])

# Always try to hide symbols
AX_APPEND_COMPILE_FLAGS([-fvisibility=hidden -pipe],[CFLAGS])
AX_APPEND_COMPILE_FLAGS([-fvisibility=hidden -fno-rtti -fno-exceptions -pipe],[CXXFLAGS])

# Always add full warnings
AX_CFLAGS_WARN_ALL
AX_CXXFLAGS_WARN_ALL

# Just for Win32 with GCC
AS_CASE([$host_os],[mingw*],
[
  AX_APPEND_COMPILE_FLAGS([-static-libgcc],[CFLAGS])
  AX_APPEND_COMPILE_FLAGS([-static-libgcc -static-libstdc++],[CXXFLAGS])
])

# Not required any more...
#AS_CASE([$host_os],[mingw*],
#[
#	# Rename libstdc++.dll.a to libstdc++.dll.a.dontlink as well!!
#	AC_CHECK_LIB([stdc++.dll],[__cxa_throw],[AC_MSG_ERROR([Rename your libstdc++.dll.a to libstdc++.dll.a.dontlink!])])
#])

# Add debug defines
AS_IF([test "x$debug" = "xtrue"],[CPPFLAGS="$CPPFLAGS -D_DEBUG"],[CPPFLAGS="$CPPFLAGS -DNDEBUG"])

# Check for the builtin intrinsics functions we use
OO_C_BUILTINS

# Check for the headers we use
AC_CHECK_HEADERS([stdint.h windows.h asl.h syslog.h unistd.h sys/socket.h malloc.h])
AC_CHECK_FUNCS([pipe2 accept4])

# Set up libtool correctly
m4_ifdef([LT_PREREQ],,[AC_MSG_ERROR([Need libtool version 2.2.6 or later])])
LT_PREREQ([2.2.6])
LT_CONFIG_LTDL_DIR([libltdl])
LT_INIT([static disable-shared])
LTDL_INIT

AC_OUTPUT
