
AM_CPPFLAGS = -Wall -Werror $(HWLOC_CFLAGS)

#######################################################
# libmpibind
#######################################################
lib_LTLIBRARIES       = libmpibind.la

libmpibind_la_SOURCES = \
    mpibind.c  mpibind-priv.h \
    internals.c \
    hwloc_utils.c hwloc_utils.h

include_HEADERS       = mpibind.h
libmpibind_la_LIBADD  = $(HWLOC_LIBS)
#libmpibind_la_CPPFLAGS = -Wall -Werror $(HWLOC_CFLAGS)

#######################################################
# Program using libmpibind and other auxiliary progs
#######################################################

noinst_PROGRAMS = main hwloc_tests

hwloc_tests_SOURCES = hwloc_tests.c hwloc_utils.c hwloc_utils.h
hwloc_tests_LDADD   = $(HWLOC_LIBS)
# Rename hwloc_utils object file since it is used by both
# a libtool library and a non-libtool target. 
hwloc_tests_CFLAGS  = $(AM_CFLAGS)
#hwloc_tests_CPPFLAGS = -Wall -Werror $(HWLOC_CFLAGS)

main_SOURCES = main.c mpibind.h 
main_LDADD   = libmpibind.la $(HWLOC_LIBS)

