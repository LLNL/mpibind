AM_CPPFLAGS = -Wall -Werror -I$(top_srcdir)/src $(HWLOC_CFLAGS) $(TAP_CFLAGS)
AM_LDFLAGS = -rpath $(TAP_LIBDIR) 
LDADD = $(top_srcdir)/src/libmpibind.la $(TAP_LIBS) $(HWLOC_LIBS)    

AM_TESTS_ENVIRONMENT = \
    export PYTHONPATH=:"$(top_srcdir)/python:$(PYTHON_SITE_PKG):$$PYTHONPATH";

TEST_EXTENSIONS = .t .py

T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
            $(top_srcdir)/config/tap-driver.sh

PY_LOG_DRIVER = $(PYTHON) $(top_srcdir)/config/tap-driver.py

coral_lassen_t_SOURCES = coral-lassen.c test_utils.c test_utils.h
epyc_corona_t_SOURCES = epyc-corona.c test_utils.c test_utils.h
coral_ea_t_SOURCES = coral-ea.c test_utils.c test_utils.h
cts1_quartz_t_SOURCES = cts1-quartz.c test_utils.c test_utils.h
error_t_SOURCES = error.c test_utils.c test_utils.h
environment_t_SOURCES = environment.c test_utils.c test_utils.h

# Fix to make tests work on macOS:
#  The tap library path is not set correctly in the executable. 
# Statement must terminate with a semicolon.
if HAVE_DARWIN_OS
AM_TESTS_ENVIRONMENT += \
	if [[ ! $@ =~ python ]]; then \
	echo "Executing dylib fix for `echo $@ | sed 's/\.log/\.t/'`"; \
	install_name_tool -change libtap.dylib \
	$(TAP_LIBDIR)/libtap.dylib .libs/`echo $@ | sed 's/\.log/\.t/'`; fi;
endif

C_TESTS = \
    error.t \
    environment.t \
    coral_lassen.t \
    epyc_corona.t \
    coral_ea.t \
    cts1_quartz.t

PYTHON_TESTS = \
    python/py-coral-ea.py \
    python/py-coral-lassen.py \
    python/py-cts1-quartz.py \
    python/py-epyc-corona.py

if HAVE_LIBTAP
TESTS = $(C_TESTS)
check_PROGRAMS = $(C_TESTS)
endif

if HAVE_CFFI
if HAVE_PYCOTAP
check_SCRIPTS = $(PYTHON_TESTS)
if HAVE_LIBTAP
TESTS += $(PYTHON_TESTS)
else
TESTS = $(PYTHON_TESTS)
endif
endif
endif
