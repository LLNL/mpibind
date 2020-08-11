from cffi import FFI
from os import path

ffi = FFI()

cdefs = """
struct hwloc_topology;
typedef struct hwloc_topology * hwloc_topology_t;
typedef struct { ...; } hwloc_bitmap_t;
"""

prepared_headers = "_mpibind_preproc.h"
if not path.exists(prepared_headers):
    prepared_headers = "_mpibind/_mpibind_preproc.h"

with open(prepared_headers) as h:
    cdefs = cdefs + h.read()

ffi.cdef(cdefs)


ffi.set_source(
    "_mpibind._mpibind",
    """
#include <src/mpibind.h>
#include <src/mpibind-priv.h>
    """,
    libraries=["mpibind"],
)


if __name__ == "__main__":
    ffi.emit_c_code("_mpibind.c")