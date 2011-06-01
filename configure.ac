AC_PREREQ(2.61)

AC_INIT(OOBase,0.5.0)
AC_CONFIG_SRCDIR([src/Buffer.cpp])
AC_CONFIG_HEADERS([autoheader_dummy.h include/oobase-autoconf.h])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES([Makefile])

AM_INIT_AUTOMAKE([foreign 1.10 no-define])

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

# Check the multi-threading flags
OO_MULTI_THREAD

# Check for the builtin intrinsics functions we use
OO_C_BUILTINS

# Check for the headers we use
AC_CHECK_HEADERS([windows.h sys/time.h asl.h syslog.h unistd.h ev.h])

# Add the correct libs to LIBS
AC_CHECK_LIB([ev],[ev_loop_new],AC_SUBST([EV_LIBS],["-lev"]))

# Check for socket options
OO_CHECK_SOCKOPT([SO_PEERCRED LOCAL_CREDS])

# Find sqlite3
OO_LIB_SQLITE3([3.5.0])

# Check for specifics of Unix cred passing
AC_CHECK_FUNCS([getpeereid getpeerucred])
AC_CHECK_TYPES([struct cmsgcred])
AC_CHECK_TYPES([struct fcred])
AC_CHECK_TYPES([struct sockcred])

# Set sensible default CFLAGS if necessary
AS_IF([test "x$oo_test_CFLAGS" != "xset"],
[
  # Always try to hide symbols
  AX_CFLAGS_GCC_OPTION([-fvisibility=hidden])
  
  AS_IF([test "x$debug" = "xtrue"],
  [
    AX_CFLAGS_GCC_OPTION([-ggdb])
    AX_CFLAGS_GCC_OPTION([-O0])
  ],
  [
    AX_CFLAGS_GCC_OPTION([-O2])
  ])
])

# Set sensible default CXXFLAGS if necessary
AS_IF([test "x$oo_test_CXXFLAGS" != "xset"],
[
  # Always try to hide symbols
  AX_CXXFLAGS_GCC_OPTION([-fvisibility=hidden])
  
  # Always add full warnings
  AX_CXXFLAGS_WARN_ALL

  AS_IF([test "x$debug" = "xtrue"],
  [
  	AX_CXXFLAGS_GCC_OPTION([-ggdb])
  	AX_CXXFLAGS_GCC_OPTION([-O0])
  ],
  [
    AX_CXXFLAGS_GCC_OPTION([-fno-rtti])
    AX_CXXFLAGS_GCC_OPTION([-O2])
  ])
])

# Add debug defines
AS_IF([test "x$debug" = "xtrue"],[CPPFLAGS="$CPPFLAGS -D_DEBUG"],[CPPFLAGS="$CPPFLAGS -DNDEBUG"])

# Set up libtool correctly
m4_ifdef([LT_PREREQ],,[AC_MSG_ERROR([Need libtool version 2.2.6 or later])])
LT_PREREQ([2.2.6])
LT_CONFIG_LTDL_DIR([libltdl])
LT_INIT([dlopen])
LTDL_INIT
	
AC_OUTPUT()
