
#######################################################
# libmpibind_flux
#######################################################

#if HAVE_FLUX_CORE
#mpibindmod_LTLIBRARIES  = mpibind_flux.la
# Not installing the lua file anymore
#mpibindmod_SCRIPTS = mpibind_flux.lua

# The mpibind plugin for Flux depends on a Flux header.
# Therefore, when using Spack, Flux is a dependency
# of mpibind. To install mpibind and Flux, one would use:
#   spack install mpibind+flux
# There's an issue though: mpibind may not be able
# to install the plugin into the flux plugins directory,
# e.g., when Flux is a system-wide distribution.
# To work around this Flux has created an environment
# variable that allows loading a plugin without writing
# it to Flux's installation directory. 
# 
# Install mpibind_flux.so into the flux shell
# plugin path so it is loaded by default with
# 'plugin.load("*.so")'. 
# plugin_name = ${mpibindmod_LTLIBRARIES:la=so}
# install-exec-hook: 
# 	$(AM_V_at)echo Installing the mpibind flux plugin...
# 	$(MKDIR_P) $(FLUX_SHELL_PLUGIN_DIR) && \
# 	$(INSTALL) $(builddir)/.libs/$(plugin_name) $(FLUX_SHELL_PLUGIN_DIR)/

# Install using a symbolic link 
# install-exec-hook: 
# 	$(AM_V_at)echo Installing the mpibind flux plugin...
# 	cd /g/g99/leon/firefall && \
# 	$(MKDIR_P) $(FLUX_SHELL_PLUGIN_DIR) && \
# 	cd $(FLUX_SHELL_PLUGIN_DIR) && \
# 	(test -e $(plugin_name) && rm $(plugin_name)) && \
# 	$(LN_S) $(mpibindmoddir)/$(plugin_name) . 
#endif

# The build directory of the plugin 
plugin_int_dir = $(abs_top_srcdir)/flux/.libs

if HAVE_FLUX_CORE
## The Flux plugin 
mpibindmod_LTLIBRARIES  = mpibind_flux.la

## Script to load the plugin--to be used by Flux
pkgdata_SCRIPTS = mpibind-flux.lua
CLEANFILES = $(pkgdata_SCRIPTS)

# Script to load the Flux plugin 
# from the installation directory
install-data-hook:
	sed -i.tmp 's|$(plugin_int_dir)|$(libdir)/mpibind|g' \
	$(pkgdatadir)/mpibind-flux.lua && \
	rm $(pkgdatadir)/mpibind-flux.lua.tmp
endif

# The Flux plugin 
mpibind_flux_la_SOURCES = plugin.c
mpibind_flux_la_CFLAGS  = -Wall -Werror -I$(top_srcdir)/src
mpibind_flux_la_CFLAGS += $(HWLOC_CFLAGS) $(FLUX_CORE_CFLAGS)
mpibind_flux_la_LIBADD  = $(top_builddir)/src/libmpibind.la
mpibind_flux_la_LDFLAGS = -module

# Script to load the Flux plugin 
# from the '.libs' directory
mpibind-flux.lua: mpibind-flux.lua.in Makefile
	sed 's|[@]fluxplugindir[@]|$(plugin_int_dir)|g' \
	mpibind-flux.lua.in > $@

# If mpibind was already installed: 
#mpibind_flux_la_CFLAGS  = -Wall -Werror $(MPIBIND_CFLAGS)
#mpibind_flux_la_LIBADD  = $(MPIBIND_LIBS)
