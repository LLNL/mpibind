

AM_CFLAGS = -Wall -Werror -I$(top_srcdir)/src $(HWLOC_CFLAGS) $(TAP_CFLAGS)
AM_LDFLAGS = -rpath $(TAP_LIBDIR)
LDADD = $(top_srcdir)/src/libmpibind.la $(TAP_LIBS)

TESTS = 

TEST_EXTENSIONS = .t

if HAVE_LIBTAP
check_PROGRAMS = $(TESTS)
endif
