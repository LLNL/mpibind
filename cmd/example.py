from _mpibind._mpibind import ffi, lib
from .wrapper import MissingFunctionError
from .mpibind_wrapper import MpibindWrapper

ntasks = 4
nthreads = 3
greedy = 0
gpu_optim = 0
smt_level = 1


def set_handle(handle, ntasks):
    lib.mpibind_set_ntasks(handle, ntasks)

def quick_example_direct():
    handle = ffi.new("struct mpibind_t *")

    set_handle(handle, ntasks)
    lib.mpibind_set_nthreads(handle, nthreads)
    lib.mpibind_set_greedy(handle, greedy)
    lib.mpibind_set_gpu_optim(handle, gpu_optim)
    lib.mpibind_set_smt(handle, smt_level)

    if lib.mpibind(handle) != 0:
        print("oh no")

    mapping_buffer = ffi.new("char[]", 2048)
    lib.mpibind_get_mapping_string(handle, mapping_buffer, 2048)
    mapping = ffi.string(mapping_buffer).decode("utf-8")
    print(mapping)



def quick_example_wrapper():
    mpibind_py = MpibindWrapper()
    handle = ffi.new("struct mpibind_t *")
    mpibind_py.set_ntasks(handle, ntasks)
    mpibind_py.set_greedy(handle, greedy)
    mpibind_py.set_gpu_optim(handle, gpu_optim)
    mpibind_py.set_smt(handle, smt_level)

    mpibind_py.use_topology('../../topo-xml/coral-lassen.xml')
    
    if mpibind_py.mpibind(handle) != 0:
        print("failure to create mpibind mapping")

     
    print(mapping)
    print(mapping.counts)

def trying_hwloc():
    topo = ffi.new("struct hwloc_topology_t *")

if __name__ == "__main__":
    quick_example_wrapper()