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

################ Python Configuration ################

# CREDIT TO THE FLUX TEAM FOR AM_CHECK_PYMOD

dnl AM_CHECK_PYMOD(MODNAME [,SYMBOL [,ACTION-IF-FOUND [,ACTION-IF-NOT-FOUND]]])
dnl Check if a module containing a given symbol is visible to python.
AC_DEFUN([AM_CHECK_PYMOD],
[AC_REQUIRE([AM_PATH_PYTHON])
py_mod_var=`echo "$1_$2" | sed 'y%<>= "./+-(),'"'"'%______p______%'`
AC_MSG_CHECKING(for ifelse([$2],[],,[$2 in ])python module $1)
AC_CACHE_VAL(py_cv_mod_$py_mod_var, [
ifelse([$2],[], [prog="
import sys
try:
        import $1
except ImportError:
        sys.exit(1)
except:
        sys.exit(0)
sys.exit(0)"], [prog="
import sys
from distutils.version import LooseVersion, StrictVersion
import $1
if not $2:
    sys.exit(1)
"])
if $PYTHON -c "$prog" 1>&AC_FD_CC 2>&AC_FD_CC
  then
    eval "py_cv_mod_$py_mod_var=yes"
  else
    eval "py_cv_mod_$py_mod_var=no"
  fi
])
py_val=`eval "echo \`echo '$py_cv_mod_'$py_mod_var\`"`
if test "x$py_val" != xno; then
  AC_MSG_RESULT(yes)
  ifelse([$3], [],, [$3
])dnl
else
  AC_MSG_RESULT(no)
  ifelse([$4], [],, [$4
])dnl
fi
])

# Set python_version to 3 if no user requested version
if test "X$PYTHON_VERSION" = "X" ; then
  if test "X$PYTHON" = "X" ; then
    PYTHON_VERSION=3
  fi
fi

# Do not let AX_PYTHON_DEVEL set PYTHON_SITE_PKG
# AX_PYTHON_DEVEL supplies the PYTHON_VERSION variable
# https://www.gnu.org/software/autoconf-archive/ax_python_devel.html
saved_PYTHON_SITE_PKG=$PYTHON_SITE_PKG
AX_PYTHON_DEVEL([>='3.6'])
PYTHON_SITE_PKG=$saved_PYTHON_SITE_PKG

# Verify that some version of python was discovered
AM_PATH_PYTHON([$ac_python_version])
if test "X$PYTHON" = "X"; then
  AC_MSG_ERROR([could not find python])
fi
if test "X$PYTHON" = "X"; then
  AC_MSG_ERROR([could not find python])
fi

# Verify that the discovered python has the required modules
AM_CHECK_PYMOD(cffi,
               [StrictVersion(cffi.__version__) >= StrictVersion('1.1.0')],
               ,
               [AC_MSG_ERROR([could not find python module cffi, version 1.1+ required])]
               )

AM_CHECK_PYMOD(wheel,
               [StrictVersion(wheel.__version__) >= StrictVersion('0.0.0')],
               ,
               [AC_MSG_ERROR([could not find python module wheel, any version should do])]
               )

# Set up locations for python bindings
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

AC_SUBST(PYTHON)

################ end Python Configuration ################

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
