from cffi import FFI

ffi = FFI()

cdefs = """
struct hwloc_topology;
typedef struct hwloc_topology * hwloc_topology_t;
typedef struct { ...; } hwloc_bitmap_t;
"""

with open("_mpibind_preproc.h") as h:
    cdefs = cdefs + h.read()

ffi.cdef(cdefs)


ffi.set_source(
    "_mpibind._mpibind",
    """
#include <src/mpibind.h>
#include <src/mpibind-priv.h>
#include <hwloc.h>
    """,
    libraries=["mpibind", "hwloc"],
)


if __name__ == "__main__":
    ffi.emit_c_code("_mpibind.c")