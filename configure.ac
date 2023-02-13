#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([edlin],[2.21],[gpietsch@comcast.net])
AC_CONFIG_SRCDIR([edlin.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([io.h jctype.h process.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_MEMCMP
AC_CHECK_FUNCS([access iskanji link memchr memmove memset rename strchr strpbrk strrchr unlink])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
