
#######################################################
# Slurm plugin
#######################################################

if HAVE_SLURM
mpibindmod_LTLIBRARIES  = mpibind_slurm.la
endif

mpibind_slurm_la_SOURCES = plugin.c
mpibind_slurm_la_CFLAGS  = -Wall -Werror -I$(top_srcdir)/src
mpibind_slurm_la_CFLAGS += $(HWLOC_CFLAGS) $(SLURM_CFLAGS)
# Need slurm/spank.h
# SLURM_CFLAGS is empty because 'pkg-config --cflags slurm'
# is empty: default paths, e.g., /usr/include, are not returned!
# Because the Slurm headers/libs are on standard locations, 
# e.g., /usr/include, everything works.
# mpibind_slurm_la_CFLAGS += -I$(SLURM_INCDIR) $(HWLOC_CFLAGS)
mpibind_slurm_la_LIBADD  = $(top_builddir)/src/libmpibind.la
mpibind_slurm_la_LDFLAGS = -module
