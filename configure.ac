#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.6])
AC_INIT([soliloquy], [0.0.1], [jasonlingle@yahoo.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AM_SILENT_RULES(yes)
AM_MAINTAINER_MODE

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_SEARCH_LIBS([pcre_compile], [pcre])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h unistd.h glob.h pcre.h getopt.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
# We never malloc() or realloc() with size zero, so we don't need these
# checks.
dnl AC_FUNC_MALLOC
dnl AC_FUNC_REALLOC
AC_FUNC_FORK
AC_TYPE_PID_T
# Don't check for regcomp as suggested by autoscan; if regex.h exists, it is
# generally safe to say that the functions defined within exist as well.
AC_CHECK_FUNCS([memmove memset strerror getopt setenv setlocale],
  [],
  AC_MSG_ERROR([A required function could not be found.]))

AC_CHECK_FUNCS([glob])

AC_CHECK_FUNCS([getopt_long],
  [AC_DEFINE([USE_GETOPT_LONG], [1], [Use getopt_long instead of getopt])],
  [])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT