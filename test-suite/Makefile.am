AM_CPPFLAGS = -Wall -Werror -I$(top_srcdir)/src $(HWLOC_CFLAGS) $(TAP_CFLAGS)
AM_LDFLAGS = -rpath $(TAP_LIBDIR) 
LDADD = $(top_srcdir)/src/libmpibind.la $(TAP_LIBS) $(HWLOC_LIBS)

TEST_EXTENSIONS = .t

T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
            $(top_srcdir)/config/tap-driver.sh

coral_lassen_t_SOURCES = coral-lassen.c test_utils.c test_utils.h
epyc_corona_t_SOURCES = epyc-corona.c test_utils.c test_utils.h
coral_ea_t_SOURCES = coral-ea.c test_utils.c test_utils.h
cts1_quartz_t_SOURCES = cts1-quartz.c test_utils.c test_utils.h
error_t_SOURCES = error.c test_utils.c test_utils.h
environment_t_SOURCES = environment.c test_utils.c test_utils.h

# Fix to make tests work on macOS.
# Todo: Make this conditional on 'darwin' platform. 
# Must terminate with a semicolon. 
#AM_TESTS_ENVIRONMENT = \
#	echo "Executing dylib fix for `echo $@ | sed 's/\.log/\.t/'`"; \
#	install_name_tool -change libtap.dylib \
#	$(TAP_LIBDIR)/libtap.dylib .libs/`echo $@ | sed 's/\.log/\.t/'`;


if HAVE_LIBTAP
TESTS = \
    error.t \
    environment.t \
    coral_lassen.t \
    epyc_corona.t \
    coral_ea.t \
    cts1_quartz.t

check_PROGRAMS = $(TESTS)
endif

