AM_CPPFLAGS = -Wall $(HWLOC_CFLAGS)
LDADD = libmpibind.la $(HWLOC_LIBS)

# Create libmpibind
lib_LTLIBRARIES = libmpibind.la

libmpibind_la_SOURCES = mpibind.c mpibind.h mpibind-priv.h
libmpibind_la_CPPFLAGS = -Wall $(HWLOC_CFLAGS)
libmpibind_la_LIBADD = $(HWLOC_LIBS)

bin_PROGRAMS = main hwloc_tests

main_SOURCES = main.c mpibind.h mpibind-priv.h
hwloc_tests_SOURCES = hwloc_tests.c mpibind.h mpibind-priv.h
