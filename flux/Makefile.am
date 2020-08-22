
#######################################################
# libmpibind_flux
#######################################################

if HAVE_FLUX_CORE
mpibindmod_LTLIBRARIES  = mpibind_flux.la
# Not installing the lua file anymore
#mpibindmod_SCRIPTS = mpibind_flux.lua

# Install mpibind_flux.so into the flux shell
# plugin path so it is loaded by default with
# 'plugin.load("*.so")'. 
plugin_name = ${mpibindmod_LTLIBRARIES:la=so}
install-exec-hook: 
	$(AM_V_at)echo Installing the mpibind flux plugin...
	$(MKDIR_P) $(FLUX_SHELL_PLUGIN_DIR) && \
	$(INSTALL) $(builddir)/.libs/$(plugin_name) $(FLUX_SHELL_PLUGIN_DIR)/

# Install using a symbolic link 
# install-exec-hook: 
# 	$(AM_V_at)echo Installing the mpibind flux plugin...
# 	cd /g/g99/leon/firefall && \
# 	$(MKDIR_P) $(FLUX_SHELL_PLUGIN_DIR) && \
# 	cd $(FLUX_SHELL_PLUGIN_DIR) && \
# 	(test -e $(plugin_name) && rm $(plugin_name)) && \
# 	$(LN_S) $(mpibindmoddir)/$(plugin_name) . 
endif

mpibind_flux_la_SOURCES = plugin.c
mpibind_flux_la_CFLAGS  = -Wall -Werror -I$(top_srcdir)/src $(HWLOC_CFLAGS)
mpibind_flux_la_LIBADD  = $(top_builddir)/src/libmpibind.la
mpibind_flux_la_LDFLAGS = -module

# If mpibind was already installed: 
#mpibind_flux_la_CFLAGS  = -Wall -Werror $(MPIBIND_CFLAGS)
#mpibind_flux_la_LIBADD  = $(MPIBIND_LIBS)
