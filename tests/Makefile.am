AM_CFLAGS= -Wall -Werror -I$(top_srcdir)/src $(HWLOC_CFLAGS)
LDADD = ../src/libmpibind.la libtap.la $(HWLOC_LIBS)

lib_LTLIBRARIES = libtap.la
libtap_la_SOURCES = tap.c tap.h

TESTS = \
    error.t \
    environment.t \
    coral_lassen.t \
    epyc_corona.t \
    coral_ea.t

check_PROGRAMS = $(TESTS)

TEST_EXTENSIONS = .t
T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
              $(top_srcdir)/config/tap-driver.sh

coral_lassen_t_SOURCES = coral-lassen.c test_utils.c test_utils.h
epyc_corona_t_SOURCES = epyc-corona.c test_utils.c test_utils.h
coral_ea_t_SOURCES = coral-ea.c test_utils.c test_utils.h
error_t_SOURCES = error.c test_utils.c test_utils.h
environment_t_SOURCES = environment.c test_utils.c test_utils.h
