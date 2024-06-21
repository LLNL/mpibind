
## The mpibind Slurm Plugin

The `mpibind_slurm.so` library is a SPANK plugin that enables using
mpibind in Slurm to map parallel codes to the hardware.

### Requirements

The file `slurm/spank.h` is necessary to build the plugin. This file is distributed with Slurm.

### Building and installing 

The building system looks for a Slurm installation using `pkg-config` and, if
found, the plugin is built and installed here:
```
<mpibind-prefix>/lib/mpibind/
# which can be obtained with the command
pkg-config --variable=plugindir mpibind
```

To install the plugin into your Slurm installation, add the following
line to the `plugstack.conf` file:
```
required <mpibind-prefix>/lib/mpibind/mpibind_slurm.so
```
The plugin configuration options are below. Separate multiple options with commas. 
```
# Disable the plugin by default
# To use mpibind add --mpibind=on to srun 
default_off

# By default, mpibind is enabled only on full-node allocations
# This option enables mpibind on partial-node allocations as well
exclusive_only_off
```
For example:
```
required <mpibind-prefix>/lib/mpibind/mpibind_slurm.so default_off
```
### Usage 

mpibind can be used with the `srun` command as follows. 

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

### Environment variables

```
# The type of resource to restrict mpibind to
MPIBIND_RESTRICT_TYPE=<cpu|mem>

# Restrict mpibind to a list of CPUs or NUMA domains
MPIBIND_RESTRICT=<list-of-integers>

# The hwloc topology file, in XML format, matching the cluster's topology
MPIBIND_TOPOFILE=<xml-file>
```

To restrict mpibind to a subset of the node resources, MPIBIND_RESTRICT must be defined with the resource IDs. Optionally, MPIBIND_RESTRICT_TYPE can be specified with the type of resource: CPUs or NUMA memory (the default is CPUs). 


For example, restrict mpibind to the third and forth NUMA domains: 

```
$ export MPIBIND_RESTRICT_TYPE=mem

$ export MPIBIND_RESTRICT=2,3

$ srun --mpibind=on -N1 -n4 ./mpi
mutt124    Task   0/  4 running on 7 CPUs: 28-34
mutt124    Task   1/  4 running on 7 CPUs: 35-41
mutt124    Task   2/  4 running on 7 CPUs: 42-48
mutt124    Task   3/  4 running on 7 CPUs: 49-55
```

To have mpibind use a topology file defining the node architecture, one can use MPIBIND_TOPOFILE. This can speed up launch time since the topology does not need to be discovered every srun command. 

Notes:
* This variable may already be defined in the user's environment. To check use `printenv MPIBIND_TOPOFILE`
* The topology file *must* match the node architecture where mpibind is run. Otherwise, the job may fail due to invalid mapping assignments.  
* To generate a topology file, run `hwloc` on a compute node as follows `lstopo <name-of-file>.xml`

For example:

```
# Generate the topology file using hwloc 
$ lstopo mutt.xml

# Run mpibind on the same node architecture as where the file was created
$ export MPIBIND_TOPOFILE=mutt.xml

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




