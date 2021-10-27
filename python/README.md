# The Python interface

`mpibind` for Python enables the use of the mpibind algorithm in
arbitrary Python programs.  

## Building and installing 

### Spack 

The easiest way to build and install the Python interface is through
`spack`
```
spack install mpibind+python
spack load mpibind 
```

### Autotools

Otherwise use the Autools process described at the top
directory. Basically, the Python bindings are built provided 
that Python 3 and CFFI are present at `configure` time. Let's assume
that mpibind's installation directory is `install_dir`

Options available to install the Python interface: 

* Add the bindings to the Python path 
```
   export PYTHONPATH=$PYTHONPATH:install_dir/share"
```
* Use `setup.py`
```
   cd install_dir/share
   python setup.py install
```

### Dependencies 

* Python 3
* The C Foreign Function Interface for Python
 ([CFFI](https://cffi.readthedocs.io/en/latest/)) 

## Usage 

Here is a toy program that demonstrates the Python interface. This
program uses `mpi4py` so make sure it is installed on your system,
e.g., `pip install mpi4py`


```python
##########
## toy.py 
##########
import os
from mpi4py import MPI
import mpibind

comm = MPI.COMM_WORLD
size = comm.Get_size()
rank = comm.Get_rank()

print(rank, ": running on cpus ", os.sched_getaffinity(0))

# Create an mpibind handle, 'ntasks' is a required parameter
handle = mpibind.MpibindHandle(ntasks=size, restrict_ids='0-7')
# Create the mapping 
handle.mpibind()

print("Applying mpibind's mapping")
handle.apply(rank)

print(rank, ": running on cpus ", os.sched_getaffinity(0))

if rank == 0:
    handle.mapping_print()
    data = [(i+1)**2 for i in range(size)]
else:
    data = None

data = comm.scatter(data, root=0)
assert data == (rank+1)**2
```


Run under `slurm` with the following command: 

```
srun -N1 -n4 python toy.py
```

Example output:
```
0 before affinity {0, 1, 2, 3, 4, 5, 6, 7, 8}
1 before affinity {9, 10, 11, 12, 13, 14, 15, 16, 17}
3 before affinity {32, 33, 34, 35, 27, 28, 29, 30, 31}
2 before affinity {18, 19, 20, 21, 22, 23, 24, 25, 26}
3 after affinity {6, 7}
2 after affinity {4, 5}
0 after affinity {0, 1}
1 after affinity {2, 3}
mpibind: task  0 nths  2 gpus  cpus 0-1
mpibind: task  1 nths  2 gpus  cpus 2-3
mpibind: task  2 nths  2 gpus  cpus 4-5
mpibind: task  3 nths  2 gpus  cpus 6-7
```

## Tests

Tests are located in the `test-suite/python` directory. We use `pycotap`
to emit the Test Anything Protocol (TAP) from Python unit
tests. Install `pycotap` before configuration to use the Python test
suite, e.g., `pip install pycotap`

Two modifications are required to add a Python test. 

1. Create a new test file under test-suite/python
2. Add the new test file to the `PYTHON_TESTS` variable in
`test-suite/Makefile.am`


## Development 

We use CFFI to build mpibind for Python.
CFFI is a Python library that allows building Python wrappers for C
code. CFFI allows for several modes of interaction between C and
Python: API vs ABI and out-of-line vs in-line. For mpibind, we use
CFFI in ABI, in-line mode. 

Exposing an mpibind C function to Python requires two modifications to
`python/mpibind.py.in`

1. Add the C function definition to the cdef argument
2. Add a wrapper for the function to the class MpibindHandle

