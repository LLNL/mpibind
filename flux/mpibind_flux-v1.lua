
-- While it requires less user intervention, as it tries to figure
-- locations and paths for the user, this script may be overly
-- complex. This file (.in) was intended to be read by autoconf
-- to generate the final lua file.


-- initrc file to load the mpibind plugin into flux.
-- To use, add the following to to the flux mini run command 
-- '-o initrc=mpibind_flux.lua' 


-- Disable Flux's 'cpu- and gpu-affinity'
--shell.options['cpu-affinity'] = "off"
--shell.options['gpu-affinity'] = "off"


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
-- Can use shell.log() or shell.debug() for output
shell.debug("Flux plugin search path: "..plugin.searchpath)

-- Look for the mpibind flux plugin in the mpibind installation
--f = assert(io.popen("pkg-config --variable=libdir mpibind"))
--s = f:read("*l")
--if s then
--  sofile = s .. "/mpibind/mpibind_flux.so"
--  if not io.open(sofile) then
--    sofile = nil 
--  end 
--end
sofile = /g/g99/leon/firefall/nick/install/lib/mpibind/mpibind_flux.so 
if not io.open(sofile) then
  sofile = nil 
end 

-- If not found, look for the MPIBIND_FLUX_PLUGIN env var
varname = "MPIBIND_FLUX_PLUGIN"
if sofile == nil then
  sofile = os.getenv(varname)
  if sofile and not io.open(sofile) then
    sofile = nil 
  end 
end

if sofile == nil then
  shell.log("Could not find mpibind flux plugin.\n" ..
  "\tMake sure /g/g99/leon/firefall/nick/install/lib/mpibind/mpibind_flux.so exists or\n" ..
  "\texport " .. varname .. "=<path>/mpibind_flux.so")
else
  shell.log("Loading plugin: "..sofile)
  plugin.load { file = sofile }
  --plugin.load { file = "./.libs/mpibind_flux.so", conf = {} }
end 





