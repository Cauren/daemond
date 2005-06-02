dnl
dnl configure.ac for daemond
dnl
dnl Bits and pieces shamelessly stolen from Underworld Adventures
dnl (Thanks, Michael and Willem)
dnl

AM_INIT_AUTOMAKE
AC_PREREQ(2.53)
AC_INIT([Daemond], [0.5], [coren@users.sourceforge.net], [daemond])
AC_CONFIG_SRCDIR([src/exec.H])

CXXCODE="-O -DRELEASE"
CXXARCH=""
CXXLANG="-frtti -fexceptions"

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_CHECK_LIB([dl], [dlopen])
AC_CHECK_LIB([cap], [capgetp])
AC_CHECK_LIB([selinux], [setexeccon])

AC_LANG_CPLUSPLUS

# Checks for programs.
AC_PROG_YACC
AC_PROG_LEX
AC_PROG_CXX

AC_CONFIG_HEADER([src/config.h])

AC_MSG_CHECKING([for debugging version])
AC_ARG_ENABLE(debug, AC_HELP_STRING(
	[--enable-debug],
	[enable debugging (also disables optimizations)]),
	[	CXXCODE="-O0 -ggdb3 -Wall -Werror"
		AC_MSG_RESULT(yes)
	], AC_MSG_RESULT(no))

CXXFLAGS="$CXXCODE $CXXARCH $CXXLANG"
AC_SUBST([CXXFLAGS])
AC_SUBST([LDFLAGS])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

