
-- initrc to load mpibind plugin into flux. To use, add '-o initrc=mpibind.lua'
-- to your flux mini run command
prefix = os.execute ("pkg-config --variable=prefix flux-core")

-- source system initrc so we also load system plugins
source_if_exists (prefix.."etc/shell/initrc.lua")

-- load mpibind plugin into flux-shell
plugin.load { file = "./.libs/libmpibindflux.so", conf = {} }
