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
* [Pycotap](https://pypi.org/project/pycotap/) (for unit testing)


## Usage 

Here is a simple [program](test-simple.py) that demonstrates the Python
interface.

```python
import os
import mpibind

# This simple example does not use MPI, thus                                    
# specify my rank and total number of tasks                                     
rank = 2
ntasks_per_node = 4

# Is sched_getaffinity supported?                                               
affinity = True if hasattr(os, 'sched_getaffinity') else False

if affinity:
    cpus = sorted(os.sched_getaffinity(0))
    affstr  = "\n>Before\n"
    affstr += "Running on {:2d} cpus: {}\n".format(len(cpus), cpus)

# Create a handle                                                               
# Num tasks is a required parameter                                             
handle = mpibind.MpibindHandle(ntasks=ntasks_per_node)

# Create the mapping                                                            
handle.mpibind()

# Print the mapping                                                             
handle.mapping_print()

# Apply the mapping as if I am worker 'rank'                                    
# This function is not supported on some platforms                              
if affinity:
    handle.apply(rank)
    cpus = sorted(os.sched_getaffinity(0))
    print(affstr + ">After\n" +
        "Running on {:2d} cpus: {}".format(len(cpus), cpus))
```

Running it on a dual-socket system with 18x2 SMT-2 cores results in the
output below. Note that the resulting mapping uses only the first socket
because `mpibind` optimizes placement for GPUs by default (configurable
parameter) and both GPUs are located on the first socket.

```bash
$ python3 test-simple.py 
mpibind: task  0 nths  4 gpus 0 cpus 0-4
mpibind: task  1 nths  4 gpus 0 cpus 5-9
mpibind: task  2 nths  4 gpus 1 cpus 10-13
mpibind: task  3 nths  4 gpus 1 cpus 14-17

>Before
Task 2: Running on 72 cpus: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71]
>After
Task 2: Running on  4 cpus: [10, 11, 12, 13]
```

A more realistic example that uses MPI is provided in
[test-mpi.py](test-mpi.py). This program uses `mpi4py` so make sure it is
installed on your system, e.g., `pip install mpi4py`. It can be run as follows:

```bash
$ srun -N2 -n8 python3 test-mpi.py
pascal7 task 0/8: lrank 0/4 nths 4 gpus ['0'] cpus [0, 1, 2, 3, 4]
pascal7 task 1/8: lrank 1/4 nths 4 gpus ['0'] cpus [5, 6, 7, 8, 9]
pascal7 task 2/8: lrank 2/4 nths 4 gpus ['1'] cpus [10, 11, 12, 13]
pascal7 task 3/8: lrank 3/4 nths 4 gpus ['1'] cpus [14, 15, 16, 17]
pascal8 task 4/8: lrank 0/4 nths 4 gpus ['0'] cpus [0, 1, 2, 3, 4]
pascal8 task 5/8: lrank 1/4 nths 4 gpus ['0'] cpus [5, 6, 7, 8, 9]
pascal8 task 6/8: lrank 2/4 nths 4 gpus ['1'] cpus [10, 11, 12, 13]
pascal8 task 7/8: lrank 3/4 nths 4 gpus ['1'] cpus [14, 15, 16, 17]
```

## Unit tests

Unit tests are located in [test-suite/python](../test-suite/python) and can be
launched from the top directory with `make check`. We use `pycotap` to emit the
Test Anything Protocol (TAP) from the Python tests. Make sure `pycotap` is
installed, e.g., `pip install pycotap` before running `configure` from the top
directory.

Two modifications are required to add a Python test. 

1. Create a new test file under [test-suite/python](../test-suite/python)
2. Add the new test file to the `PYTHON_TESTS` variable in
[test-suite/Makefile.am](../test-suite/Makefile.am)


## Development 

We use CFFI to build mpibind for Python.
CFFI is a Python library that allows building Python wrappers for C
code. CFFI allows for several modes of interaction between C and
Python: API vs ABI and out-of-line vs in-line. For mpibind, we use
CFFI in ABI, in-line mode. 

Exposing an mpibind C function to Python requires two modifications to
[mpibind.py.in](mpibind.py.in)

1. Add the C function definition to the `cdef` argument
2. Add a wrapper for the function to the class `MpibindHandle`

