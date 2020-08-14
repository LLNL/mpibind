AM_CPPFLAGS = -Wall -Werror -I$(top_srcdir)/src $(HWLOC_CFLAGS) $(TAP_CFLAGS)
AM_LDFLAGS = -rpath $(TAP_LIBDIR) 
LDADD = $(top_srcdir)/src/libmpibind.la $(TAP_LIBS) $(HWLOC_LIBS)

TEST_EXTENSIONS = .t .py

# C Tests

T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
			$(top_srcdir)/config/tap-driver.sh

PROGS = \
	error.t \
	environment.t \
	coral_lassen.t \
	epyc_corona.t \
	coral_ea.t \
	cts1_quartz.t

coral_lassen_t_SOURCES = coral-lassen.c test_utils.c test_utils.h
epyc_corona_t_SOURCES = epyc-corona.c test_utils.c test_utils.h
coral_ea_t_SOURCES = coral-ea.c test_utils.c test_utils.h
cts1_quartz_t_SOURCES = cts1-quartz.c test_utils.c test_utils.h
error_t_SOURCES = error.c test_utils.c test_utils.h
environment_t_SOURCES = environment.c test_utils.c test_utils.h

if HAVE_LIBTAP
check_PROGRAMS = $(PROGS)
endif

# Python Tests
PY_LOG_DRIVER = $(PYTHON) $(top_srcdir)/config/tap-driver.py

PYSCRIPTS = \
	python/wrapper.py

AM_TESTS_ENVIRONMENT = \
	export PYTHONPATH="$(abs_top_builddir)/bindings/python:$(abs_top_builddir)/test-suite/python::$$PYTHONPATH";\
	export PYTHON="${PYTHON}";\
	cp $(top_srcdir)/test-suite/tap-driver.py $(top_srcdir)/config/tap-driver.py;

# Run both Python and C Tests
TESTS = \
	$(PROGS)

# $(PYSCRIPTS)

clean-local:
	rm -rf trash-directory.* test-results .prove *.broker.log */*.broker.log *.output python/__pycache__