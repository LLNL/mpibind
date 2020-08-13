from cffi import FFI
from os import path

ffibuilder = FFI()

# add some of the opaque hwloc objects
cdefs = """
typedef struct { ...; } hwloc_topology_t;
typedef struct { ...; } hwloc_bitmap_t;
"""

# this file is used from two different places during build
# and then during distribution
prepared_headers = "_mpibind_preproc.h"
if not path.exists(prepared_headers):
    prepared_headers = "_mpibind/_mpibind_preproc.h"

# add cleaned header file to the definitions we expose to python
with open(prepared_headers) as h:
    cdefs = cdefs + h.read()

# set the defintions we expose to python
ffibuilder.cdef(cdefs)

# indicate that the library makes the definitions we set
ffibuilder.set_source(
    "_mpibind._pympibind",
    """
    #include <src/mpibind.h>
    """,
    libraries=["mpibind"],
)

#emit c code and let autotools build _pympibind.so extension module
if __name__ == "__main__":
    ffibuilder.emit_c_code("_pympibind.c")