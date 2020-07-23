
-- source system initrc so we also load system plugins
source_if_exists "/g/g16/hardy25/flux-core/src/shell/initrc.lua"

-- load mpibind plugin into flux-shell
plugin.load { file = "./mpibind.so", conf = {smt = 1,
                                            greedy= 0,
                                            gpu_optim = 1} }
