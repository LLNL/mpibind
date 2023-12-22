
### The mpibind Slurm Plugin

The `mpibind_slurm.so` plugin is a SPANK plugin that enables using
the mpibind algorithm in Slurm to map parallel codes to the
hardware.

#### Requirements

The file `slurm/spank.h` is necessary to build the plugin. This file is part of the Slurm installation. 

#### Building and installing 

The building system looks for a Slurm installation using `pkg-config` and, if
found, the plugin is built and installed here:
```
<mpibind-prefix>/lib/mpibind/
# which can be obtained with the command
pkg-config --variable=plugindir mpibind
```

To install the plugin into your Slurm installation add the following
line to the `plugstack.conf` file:
```
required <mpibind-prefix>/lib/mpibind/mpibind_slurm.so
```

#### Usage 

```
Automatically map tasks/threads/GPU kernels to heterogeneous hardware

Usage: --mpibind=[args]
  
where args is a comma separated list of one or more of the following:
  gpu[:0|1]         Enable(1)/disable(0) GPU-optimized mappings
  greedy[:0|1]      Allow(1)/disallow(0) multiple NUMAs per task
  help              Display this message
  off               Disable mpibind
  on                Enable mpibind
  smt:<k>           Enable worker use of SMT-<k>
  v[erbose]         Print affinty for each task
```

For example:

```
# Turn off mpibind to check the mapping without it
$ srun -N1 -n8 --mpibind=off ./mpi
mutt29     Task   0/  8 running on 224 CPUs: 0-223
mutt29     Task   1/  8 running on 224 CPUs: 0-223
mutt29     Task   2/  8 running on 224 CPUs: 0-223
mutt29     Task   3/  8 running on 224 CPUs: 0-223
mutt29     Task   4/  8 running on 224 CPUs: 0-223
mutt29     Task   5/  8 running on 224 CPUs: 0-223
mutt29     Task   6/  8 running on 224 CPUs: 0-223
mutt29     Task   7/  8 running on 224 CPUs: 0-223

# mpibind should be on by default, but can by enabled explicitly
$ srun -N1 -n8 --mpibind=on ./mpi
mutt29     Task   0/  8 running on 14 CPUs: 0-13
mutt29     Task   1/  8 running on 14 CPUs: 14-27
mutt29     Task   2/  8 running on 14 CPUs: 28-41
mutt29     Task   3/  8 running on 14 CPUs: 42-55
mutt29     Task   4/  8 running on 14 CPUs: 56-69
mutt29     Task   5/  8 running on 14 CPUs: 70-83
mutt29     Task   6/  8 running on 14 CPUs: 84-97
mutt29     Task   7/  8 running on 14 CPUs: 98-111

# Get the mapping from mpibind itself
$ srun -N1 -n8 --mpibind=v /bin/true 
mpibind: 0 GPUs on this node
mpibind: task  0 nths 14 gpus  cpus 0-13
mpibind: task  1 nths 14 gpus  cpus 14-27
mpibind: task  2 nths 14 gpus  cpus 28-41
mpibind: task  3 nths 14 gpus  cpus 42-55
mpibind: task  4 nths 14 gpus  cpus 56-69
mpibind: task  5 nths 14 gpus  cpus 70-83
mpibind: task  6 nths 14 gpus  cpus 84-97
mpibind: task  7 nths 14 gpus  cpus 98-111
```



