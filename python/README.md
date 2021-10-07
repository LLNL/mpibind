## mpibind for Python

mpibind for Python enables the use of the mpibind algorithm in arbitrary python programs.

### Building and installing 

The python bindings will be built using the normal build process:

```
$ ./bootstrap

$ ./configure --prefix=<install_dir>

$ make

$ make install
```

provided that python3 and CFFI are present at configure time.

Users of the bindings have 2 options when it comes to installation:

1. Add the bindings to the python path 
```
   export PYTHONPATH="${PYTHONPATH}:<install_dir>/share"
```
2. Install the bindings
```
   cd <install_dir>/share
   python setup.py install
```

### Usage 

Here is a toy example that demonstrates the mpibind for Python interface

```python
from mpi4py import MPI
import mpibind
import os

comm = MPI.COMM_WORLD
size = comm.Get_size()
rank = comm.Get_rank()

print(rank, "before affinity", os.sched_getaffinity(0))
handle = mpibind.MpibindHandle(ntasks=size, restrict_ids='0-7')
handle.mpibind()
handle.apply(rank)
print(rank, "after affinity", os.sched_getaffinity(0))

if rank == 0:
    print(handle.mapping_snprint())
    data = [(i+1)**2 for i in range(size)]
else:
    data = None


data = comm.scatter(data, root=0)
assert data == (rank+1)**2
```

Install dependencies (make sure to install mpibind too!)
```
pip install mpi4py
```

Run under slurm with

```
srun -u -N1 -n4 python mpibind_example.py
```

Output (from a quartz debug node):
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

### Development and Testing

We use the C Foreign Function Interface for Python or [CFFI](https://cffi.readthedocs.io/en/latest/) to build mpibind for Python.
CFFI is a Python library that allows us to build python wrappers for c code. CFFI allows for several modes of interaction between C and Python (API vs ABI) and (out-of-line vs in-line). For mpibind, we use CFFI in in-line ABI mode.

Exposing an mpibind function to python requires two modifications to the python/mpibind.py.in.

1. Add the C function definition to the cdef argument
2. Add a wrapper for the function to the class MpibindHandle

Tests are located in the test-suite/python directory. We use pycotap to emit the Test Anything Protocol (TAP) from python unit tests. Install pycotap before configuration to use the python test suite.
```
pip install pycotap
```

Two modifications are required to add a python test.

1. Create a new test file under test-suite/python
2. Add the new test file to the PYTHON_TESTS variable in test-suite/Makefile.am