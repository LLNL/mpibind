from _mpibind._mpibind import ffi, lib
from .wrapper import MissingFunctionError
from .mpibind_wrapper import MpibindWrapper

ntasks = 4
nthreads = 3
greedy = 0
gpu_optim = 0
smt_level = 1


def quick_example_direct():
    handle = ffi.new("struct mpibind_t *")

    lib.mpibind_set_ntasks(handle, ntasks)
    lib.mpibind_set_nthreads(handle, nthreads)
    lib.mpibind_set_greedy(handle, greedy)
    lib.mpibind_set_gpu_optim(handle, gpu_optim)
    lib.mpibind_set_smt(handle, smt_level)

    if lib.mpibind(handle) != 0:
        print("oh no")

    lib.mpibind_print_mapping(handle)


def quick_example_wrapper():
    mpibind_py = MpibindWrapper()
    handle = ffi.new("struct mpibind_t *")
    mpibind_py.set_ntasks(handle, ntasks)
    mpibind_py.set_greedy(handle, greedy)
    mpibind_py.set_gpu_optim(handle, gpu_optim)
    mpibind_py.set_smt(handle, smt_level)
    
    if mpibind_py.mpibind(handle) != 0:
        print("failure to create mpibind mapping")

    mpibind_py.print_mapping(handle)
    
def example_error():
    try:
        mpibind_py.function_does_not_exist()
    except MissingFunctionError:
        print("Do something if function does not exist")

def trying_hwloc():
    topo = ffi.new("struct hwloc_topology_t *")

if __name__ == "__main__":
    quick_example_wrapper()