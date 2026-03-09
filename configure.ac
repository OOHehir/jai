AC_INIT([jai], [0.1], [https://www.scs.stanford.edu/~dm/addr/])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])

: ${CXXLANG=-std=gnu++23}

AC_PROG_CXX

# Require C++23
CXXFLAGS="$CXXFLAGS $CXXLANG"
AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM([], [])],
  [],
  [AC_MSG_ERROR([a C++23 compiler is required])])
AC_LANG_POP([C++])

PKG_CHECK_MODULES([MOUNT], [mount])
PKG_CHECK_MODULES([LIBACL], [libacl])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
