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
AC_PROG_CXX
AM_PROG_CC_C_O

# Checks for libraries.
AC_SEARCH_LIBS([pcre_compile], [pcre], [],
               [AC_MSG_ERROR(
[The PCRE library could not be found. Make sure libpcre[-dev] is installed.])])
AC_SEARCH_LIBS([GC_init], [gc], [],
               [AC_MSG_ERROR(
[The gc library could not be found. Make sure libgc[-dev] is installed.])])
AC_SEARCH_LIBS([wget_wch], [ncursesw curses cursesw ncurses pcurses], [],
               [AC_MSG_ERROR(
[No wide-character version of curses could be found. You probably want to
install libncursesw5-dev.])])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h unistd.h glob.h pcre.h getopt.h locale.h],
                 [],
                 [AC_MSG_ERROR([A required header could not be found.])])

AC_CHECK_HEADERS([gc.h gc/gc.h])
AC_CHECK_HEADERS([ncursesw/cursesw.h ncurses/cursesw.h ncursesw/curses.h cursesw.h curses.h])

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

AC_CHECK_FUNCS([use_default_colors])
AC_CHECK_FUNCS([ppoll])
AC_CHECK_FUNCS([wcsdup strlcpy wcslcpy strlcat wcslcat])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
