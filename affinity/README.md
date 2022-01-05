
### Report the mapping of workers to the hardware

These programs report the mapping of CPUs and GPUs for each process
and thread. There are three variants:

* MPI: `mpi`
* OpenMP: `omp`
* MPI+OpenMP: `mpi+omp`

#### Running

Usage is straightforward. Use the `-v` option for verbose GPU output and
the `-h` option for help.

```
$ srun -n4 ./mpi
node173  Task   0/  4 running on 4 CPUs: 0,3,6,9
         Task   0/  4 has 2 GPUs: 0x63 0x43 
node173  Task   1/  4 running on 4 CPUs: 12,15,18,21
         Task   1/  4 has 2 GPUs: 0x3 0x27 
node173  Task   2/  4 running on 4 CPUs: 24,27,30,33
         Task   2/  4 has 2 GPUs: 0xe3 0xc3 
node173  Task   3/  4 running on 4 CPUs: 36,39,42,45
         Task   3/  4 has 2 GPUs: 0x83 0xa3 
```

```
$ srun -n4 ./mpi -v
node173  Task   0/  4 running on 4 CPUs: 0,3,6,9
         Task   0/  4 has 2 GPUs: 0x63 0x43 
	Default device: 0x63
	0x63: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
	0x43: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
node173  Task   1/  4 running on 4 CPUs: 12,15,18,21
         Task   1/  4 has 2 GPUs: 0x3 0x27 
	Default device: 0x3
	0x03: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
	0x27: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
node173  Task   2/  4 running on 4 CPUs: 24,27,30,33
         Task   2/  4 has 2 GPUs: 0xe3 0xc3 
	Default device: 0xe3
	0xe3: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
	0xc3: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
node173  Task   3/  4 running on 4 CPUs: 36,39,42,45
         Task   3/  4 has 2 GPUs: 0x83 0xa3 
	Default device: 0x83
	0x83: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
	0xa3: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
```

```
$ OMP_NUM_THREADS=4 srun -n2 ./omp
Process running on 1 CPUs: 0
Process has 4 GPUs: 0x63 0x43 0x3 0x27 
	Default device: 0x63
	0x63: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
	0x43: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
	0x03: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
	0x27: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
Thread   0/  4 running on 1 CPUs: 0
Thread   0/  4 assigned to GPU: 0x63
Thread   1/  4 running on 1 CPUs: 6
Thread   1/  4 assigned to GPU: 0x43
Thread   2/  4 running on 1 CPUs: 12
Thread   2/  4 assigned to GPU: 0x3
Thread   3/  4 running on 1 CPUs: 18
Thread   3/  4 assigned to GPU: 0x27

Process running on 1 CPUs: 24
Process has 4 GPUs: 0xe3 0xc3 0x83 0xa3 
	Default device: 0xe3
	0xe3: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
	0xc3: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
	0x83: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
	0xa3: Vega 20, 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.6 CC
Thread   0/  4 running on 1 CPUs: 24
Thread   0/  4 assigned to GPU: 0xe3
Thread   1/  4 running on 1 CPUs: 30
Thread   1/  4 assigned to GPU: 0xc3
Thread   2/  4 running on 1 CPUs: 36
Thread   2/  4 assigned to GPU: 0x83
Thread   3/  4 running on 1 CPUs: 42
Thread   3/  4 assigned to GPU: 0xa3
```

#### Building

These program are built with a single Makefile. By default typing
`make` will only build the CPU-related programs. To enable GPU
information, the user needs to set an environment variable.

```
# Build with CPU support
$ make -f makefile.mk

# Build with support for AMD GPUs
$ HAVE_AMD_GPUS=1 make -f makefile.mk 

# Build with support for NVIDIA GPUs
$ HAVE_NVIDIA_GPUS=1 make -f makefile.mk
```

To build with AMD GPU support, the ROCm environment must be
present. Similarly, for NVIDIA support, the CUDA environment must be
present. 







