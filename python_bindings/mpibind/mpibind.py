from cffi import FFI

def abi_inline():
    ffi = FFI()
    ffi.cdef("""
        struct mpibind_t; 
        typedef struct mpibind_t mpibind_t;
        int mpibind_init(mpibind_t **handle);
    """)
    C = ffi.dlopen("mpibind")

if __name__ == "__main__":
    abi_inline()