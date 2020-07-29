AC_INIT([mpibind], [1.0.1])

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])

AC_REQUIRE_AUX_FILE([tap-driver.sh])

# Automake
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AM_SILENT_RULES([yes])

AM_CONFIG_HEADER([config/config.h])
AM_MAINTAINER_MODE([enable])


AC_ENABLE_SHARED
AC_DISABLE_STATIC
LT_INIT

# //Code coverage not yet configured
# AX_CODE_COVERAGE
# AS_IF([test x$enable_code_coverage = xyes], [
#    AC_DEFINE([CODE_COVERAGE_ENABLED], [1], [code coverage support])])

# Checks for programs.
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_AWK

# Constants to substitute
HWLOC_LIBS = $(pkg-config --libs hwloc)
HWLOC_CFLAGS $(pkg-config --cflags hwloc)
AC_SUBST([HWLOC_LIBS])
AC_SUBST([HWLOC_CFLAGS])

TOPOLOGY_DIR = ./topo-xml
AC_SUBST([TOPOLOGY_DIR])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h sys/param.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_RESTRICT
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([regcomp strchr strerror])

PKG_CHECK_MODULES([HWLOC], [hwloc >= 2.0.1], [], [])

AC_CONFIG_FILES([
    Makefile
    src/Makefile
])

AC_OUTPUT
