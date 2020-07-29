# Create libmpibind
lib_LTLIBRARIES = libmpibind.la

libmpibind_la_SOURCES = mpibind.c mpibind.h mpibind-priv.h
libmpibind_la_CPPFLAGS = -Wall $(HWLOC_CFLAGS)
libmpibind_la_LIBADD = $(HWLOC_LIBS)

# main file
bin_PROGRAMS = main
main_SOURCES = main.c
main_CPPFLAGS = -Wall $(HWLOC_CFLAGS)
main_LDADD = libmpibind.la $(HWLOC_LIBS)