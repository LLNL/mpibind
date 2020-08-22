-- initrc file to load the mpibind plugin into flux.
-- To use, add the following to to the flux mini run command 
-- '-o initrc=mpibind_flux.lua' 


-- This construct allows a site to set default mpibind parameters.
-- Note that by default when the plugin is loaded, mpibind is on
-- even when '-o mpibind' is not used. 
--if not shell.options.mpibind then
--    shell.options.mpibind = on
--end


-- Load the system initrc.lua to get the system plugins 
-- Todo: In the future, this won't be necessary: 
-- Use '-o userrc=mpibind.lua' instead of '-o initrc=mpibind.lua.
-- https://github.com/flux-framework/flux-core/pull/3132
source_if_exists(os.getenv("FLUX_DIR").."/etc/flux/shell/initrc.lua")


-- Load the mpibind plugin into flux-shell
shell.debug("Flux plugin search path: "..plugin.searchpath)
-- sofile = <mpibind-prefix>/lib/mpibind/mpibind_flux.so
sofile = "./.libs/mpibind_flux.so"
shell.debug("Loading plugin: " .. sofile)
plugin.load { file = sofile, conf = {} }

