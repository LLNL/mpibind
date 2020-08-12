AM_CFLAGS = -Wall -Werror -I$(top_srcdir)/src $(HWLOC_CFLAGS) #-fpic -shared
AM_LDFLAGS = -L$(top_srcdir)/src/ -lmpibind
LDADD = $(top_srcdir)/src/libmpibind.la

#######################################################
# libmpibindflux
#######################################################
libmpibindflux_la_SOURCES = mpibind_plugin.c
libmpibindflux_la_LIBADD  = $(HWLOC_LIBS)
libmpibindflux_la_CPPFLAGS = -Wall -Werror

if HAVE_FLUX_CORE
lib_LTLIBRARIES       = libmpibindflux.la
endif