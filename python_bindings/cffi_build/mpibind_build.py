from cffi import FFI
ffibuilder = FFI()
source = """
typedef int... hwloc_topology_t;
typedef int... hwloc_bitmap_t;
"""
with open("_mpibind.h") as h:
    source += h.read()

# set_source() gives the name of the python extension module to
# produce, and some C source code as a string.  This C code needs
# to make the declarated functions, types and globals available,
# so it is often just the "#include".
ffibuilder.set_source("cffi_build._mpibind_cffi", 
     """
     #include \"_mpibind.h\"
     #include <hwloc.h>
     """, 
     libraries=['mpibind', 'hwloc'])


ffibuilder.cdef(source)

if __name__ == "__main__":
    #ffibuilder.emit_c_code('test.c')
    ffibuilder.compile(verbose=True)