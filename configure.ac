## Prologue
# Todo: Derive version automatically from git stable release tag
AC_INIT([mpibind], [0.1.0])
AC_CONFIG_MACRO_DIRS([config])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADERS([config/config.h])


## Automake support
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AM_SILENT_RULES([yes])


## Check for programs 
# pkg-config is discovered through PKG_PROG_PKG_CONFIG, 
# which detects the presence and version of pkg-config itself.
# Need to make sure that pkg.m4 is in ACLOCAL_PATH, e.g, 
# ACLOCAL_PATH=$ACLOCAL_PATH:PKG_CONFIG_ROOT/share/aclocal
# aclocal --print-ac-dir
# Todo: Create pkg-config vars for mpibind
PKG_PROG_PKG_CONFIG

# libtool 
LT_INIT
#AC_DISABLE_SHARED
#AC_DISABLE_STATIC

AC_PROG_CC_C99
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_AWK

# Constants to substitute
HWLOC_LIBS=$(pkg-config --libs hwloc)
HWLOC_CFLAGS=$(pkg-config --cflags hwloc)
HWLOC_INCLUDEDIR=$(pkg-config --variable=includedir hwloc) 
AC_SUBST([HWLOC_LIBS])
AC_SUBST([HWLOC_CFLAGS])
AC_SUBST([HWLOC_INCLUDEDIR])

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

################ Python Configuration ################

#  Edit PATH to remove $PWD/src/cmd so that AM_PATH_PYTHON doesn't find
#  mpibind python script (thus creating a link to itself.) This needs to be
#  done *before* AX_PYTHON_DEVEL.
#
saved_PATH=$PATH
export PATH=$(echo $PATH | sed "s|$(pwd)/src/cmd:*||")

if test "X$PYTHON_VERSION" = "X" ; then
  if test "X$PYTHON" = "X" ; then
    # if the user hasn't specified, try for python 3
    PYTHON_VERSION=3
  fi
fi

# Do not let AX_PYTHON_DEVEL set PYTHON_SITE_PKG
saved_PYTHON_SITE_PKG=$PYTHON_SITE_PKG
AX_PYTHON_DEVEL([>='3.6'])
PYTHON_SITE_PKG=$saved_PYTHON_SITE_PKG

AM_PATH_PYTHON([$ac_python_version])
if test "X$PYTHON" = "X"; then
  AC_MSG_ERROR([could not find python])
fi
if test "X$PYTHON" = "X"; then
  AC_MSG_ERROR([could not find python])
fi
#  Restore original PATH:
export PATH=${saved_PATH}

# Flag for PYTHON_LDFLAGS workaround below.
if test -n "$PYTHON_LDFLAGS"; then
  ac_python_ldflags_set_by_user=true
fi

AM_CHECK_PYMOD(cffi,
               [cffi.__version_info__ >= (1,10)],
               ,
               [AC_MSG_ERROR([could not find python module cffi, version 1.1+ required])]
               )
AM_CHECK_PYMOD(six,
               [StrictVersion(six.__version__) >= StrictVersion('1.9.0')],
               ,
               [AC_MSG_ERROR([could not find python module six, version 1.9.0+ required])]
               )
AM_CHECK_PYMOD(yaml,
               [StrictVersion(yaml.__version__) >= StrictVersion ('3.10.0')],
               ,
               [AC_MSG_ERROR([could not find python module yaml, version 3.10+ required])]
               )
AM_CHECK_PYMOD(jsonschema,
               [StrictVersion(jsonschema.__version__) >= StrictVersion ('2.3.0')],
               ,
               [AC_MSG_ERROR([could not find python module jsonschema, version 2.3.0+ required])]
               )

# Remove -L<path> from PYTHON_LDFLAGS if it is in a standard path
# (e.g. /usr/lib64).  Placing a standard path earlier in the linker
# search can lead to linking problems.
#
# Logic below assumes only newer Python versions, protected by
# above check for atleast Python 3.6.
if test "$ac_python_ldflags_set_by_user" != "true"; then
  AC_CHECK_LIB([$ac_python_library], [PyArg_ParseTuple],
               [ac_python_in_ld_path=true])
  if test "$ac_python_in_ld_path" = "true"; then
    AC_MSG_NOTICE([Removing -L$ac_python_libdir from PYTHON_LDFLAGS])
    PYTHON_LDFLAGS="-l$ac_python_library"
  fi
fi
AS_VAR_SET(mpibindpydir, $pyexecdir/mpibind)
AC_SUBST(mpibindpydir)
AS_VAR_SET(mpibindpysodir, $pyexecdir/_mpibind)
AC_SUBST(mpibindpysodir)
AC_SUBST(PYTHON_LIBRARY, lib${ac_python_library}.so)

AS_VAR_SET(mpibindlibdir, $libdir/mpibind)
AC_SUBST(mpibindlibdir)
AS_VAR_SET(mpibindpylinkdir, $mpibindlibdir/python$PYTHON_VERSION)
AC_SUBST(mpibindpylinkdir)

AS_VAR_SET(mpibindbindingincludedir, $includedir/mpibind/_binding)
AC_SUBST(mpibindbindingincludedir)

AC_DEFINE_UNQUOTED([PYTHON_INTERPRETER], ["$PYTHON"], [The python interpreter mpibind is configured with])
AC_SUBST(PYTHON)

AC_ARG_ENABLE([pylint],
  [AS_HELP_STRING([--enable-pylint],
    [Enable pylint checks of python bindings])],,
  [enable_pylint="no"]
)
AS_IF([test "x$enable_pylint" = "xyes"], [
  AC_CHECK_PROG(PYLINT,[pylint],[pylint])
  AS_IF([test "x$PYLINT" != "xpylint"], [AC_MSG_ERROR([No pylint found in PATH])])
  AM_CHECK_PYMOD(pylint,
                   [StrictVersion(pylint.__version__) >= StrictVersion('1.8.4')],
                   ,
                   [AC_MSG_ERROR([could not find python module pylint, version 1.8.4+ required])]
                 )
])
AM_CONDITIONAL([ENABLE_PYLINT], [test "x$PYLINT" = "xpylint"])



# checks for Modules

PKG_CHECK_MODULES([HWLOC], [hwloc >= 2.0.1], [], [])

# hwloc 
PKG_CHECK_MODULES([HWLOC], [hwloc >= 2.1])

# libtap
# In newer versions of pkgconf, I could use
# PKG_HAVE_WITH_MODULES and PKG_CHECK_VAR.
# I could also request min-version above with
# PKG_PROG_PKG_CONFIG([MIN-VERSION])
PKG_CHECK_MODULES([TAP], [tap], [
  libtap=true
  libtap_dir=`$PKG_CONFIG --variable=libdir tap`
  AC_SUBST(TAP_LIBDIR, $libtap_dir)
  AC_REQUIRE_AUX_FILE([tap-driver.sh])
  ],
  [AC_MSG_NOTICE([Test suite will not be built])]
)
AM_CONDITIONAL([HAVE_LIBTAP], [test x$libtap = xtrue])
# PKG_CHECK_VAR([TAP_LIBDIR], [tap], [libdir],
#   [libtap_libdir=true],
#   [AC_MSG_NOTICE([TAP's libdir not found])]
# )

# flux-core
PKG_CHECK_MODULES([FLUX_CORE], [flux-core],
  [fluxcore=true],
  [AC_MSG_NOTICE([Flux plugin will not be built])]
)
AM_CONDITIONAL([HAVE_FLUX_CORE], [test x$fluxcore = xtrue])

# Notes 
# AC_DEFINE([HAVE_LIBTAP], 1, [Define libtap to build the test suite])
# AC_CHECK_FUNC([hwloc_topology_set_all_types_filter])
# AC_CHECK_HEADERS([pkgconf/libpkgconf/libpkgconf.h])
# defines HAVE_LIBTAP and prepends -ltap to LIBS
# AC_CHECK_LIB(tap, plan)
# AC_SEARCH_LIBS([pkgconf_pkg_free], [pkgconf],
#  [AC_MSG_NOTICE([Greetings from pkgconf])],
#  [AC_MSG_ERROR([unable to find pkgconf_pkg_free()])
# ])
# AS_IF(test-1, [run-if-true-1], ..., [run-if-false])

# Define pkgconfigdir to install mpibind.pc
PKG_INSTALLDIR


## Epilogue 
AC_CONFIG_FILES([
 Makefile
 src/Makefile
 test-suite/Makefile
 etc/Makefile
 etc/mpibind.pc
 bindings/Makefile
 bindings/python/Makefile
 bindings/python/_mpibind/Makefile
 bindings/python/mpibind/Makefile
])

AC_OUTPUT
