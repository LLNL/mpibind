
# Affinity with mpibind

*Edgar A. Leon* and *Jane E. Herriman*<br>
Lawrence Livermore National Laboratory


## Table of contents

1. [Machine topology](#machine-topology)
1. [Is mpibind enabled?](#is-mpibind-enabled)
1. [If nothing else, ask for the whole node!](#if-nothing-else-ask-for-the-whole-node)
1. [Remote memory accesses is a no, no,
    but...](#remote-memory-accesses-is-a-no-no-but)
1. [I want more (hardware threads)!](#i-want-more-hardware-threads)
1. [Let me drive (your OpenMP)](#let-me-drive-your-openmp)
1. [How about system noise?](#how-about-system-noise)
1. [References](#references)


<br>

---
<br>

![Lassen](../figures/sierra.png ":Lassen")

<br>

`mpibind` is a memory-driven algorithm to map parallel hybrid
applications to the underlying hardware resources transparently,
efficiently, and portably. Unlike other mappings, its primary design
point is the memory system, including the cache hierarchy. Compute
elements are selected based on a memory mapping and not vice versa. In
addition, mpibind embodies a global awareness of hybrid programming
abstractions as well as heterogeneous systems with accelerators.

Key design principles:

* Generate mappings automatically with minimum user input
* Maximize cache and memory per worker
* Leverage resource locality based on node topology
* Optimize mappings based on a given type of resource
* Minimize remote memory accesses


In this document, we focus on the following options: 

```
-o mpibind=off|on
-o mpibind=verbose:0|1
-o mpibind=greedy:0|1
-o mpibind=smt:<n>
-o mpibind=gpu_optim:0|1
```

Multiple options can be specified by using commas, e.g.:

```
-o mpibind=verbose:1,smt:2
```



## Machine topology

Before talking about binding and affinity, let's make sure we
understand the machine topology. There are a few example
architectures [here](../common/archs.md). In this document, we focus on the
`Tioga` architecture.

Tioga has 4 NUMA domains, each with 16 cores, 2 `L3` caches and 2
GPUs. Each GPU is closest (local) to the 8 cores with which it shares an `L3`
cache.  This is shown in the following schematic that we'll be using to visualize a node on Tioga:

<p align="center">
   <img src="../figures/tioga/figureA.png" width="550"/>
</p>

Each of the four panels represents a NUMA domain; the resources in each row are associated with a different `L3` cache. Dashed lines separate the two hardware threads associated with each core. Later, tasks will be shown as colored rectangles that span one or more cores or GPUs (with different colors representing distinct tasks).

Since the affinity programs used in this document use PCI IDs to
identify GPUs, the following table provides the equivalent ordinal
numbers and their associated NUMA domains.


<table style="text-align:center;margin-left:auto;margin-right:auto;">
  <tr>
    <td colspan="2">NUMA 0</td>
    <th colspan="2">NUMA 1</th>
    <td colspan="2">NUMA 2</td>
    <td colspan="2">NUMA 3</td>
  </tr>
  <tr>
    <td>L3</td>
    <td>L3</td>
    <th>L3</th>
    <th>L3</th>
    <td>L3</td>
    <td>L3</td>
    <th>L3</th>
    <td>L3</td>
  </tr>
  <tr>
    <td>Cores<br>0-7</td>
    <td>Cores<br>8-15</td>
    <th>Cores<br>16-23</th>
    <th>Cores<br>24-31</th>
    <td>Cores<br>32-39</td>
    <td>Cores<br>40-47</td>
    <th>Cores<br>48-55</th>
    <td>Cores<br>56-63</td>
  </tr>
  <tr>
    <td>GPU 4<br>0xd1</td>
    <td>GPU 5<br>0xd6</td>
    <th>GPU 2<br>0xc9</th>
    <th>GPU 3<br>0xce</th>
    <td>GPU 6<br>0xd9</td>
    <td>GPU 7<br>0xde</td>
    <th>GPU 0<br>0xc1</th>
    <td>GPU 1<br>0xc6</td>
  </tr>
</table>



## Is mpibind enabled?

If `mpibind` is enabled on your system, you should see the
mappings when enabling verbosity:

<details>
<summary>

```
$ flux run -N1 -n2 -o mpibind=verbose:1 /bin/true
```
</summary>

```
0.447s: flux-shell[0]: mpibind:
mpibind: task  0 nths  1 gpus 1 cpus 62
mpibind: task  1 nths  1 gpus 1 cpus 63
```
</details>

If, on the other hand, the output is empty, then you need to tell Flux
to use mpibind (see [Installing the Flux Plugin](../../flux/README.md#installing-the-plugin-in-flux) for more information). Here's one way to do it:

```
export FLUX_SHELL_RC_PATH=<mpibind-prefix-dir>/share/mpibind:$FLUX_SHELL_RC_PATH
```

Where `<mpibind-prefix-dir>` is the top-level directory of your
[mpibind installation](#references).

For example:
```
export FLUX_SHELL_RC_PATH=/collab/usr/global/tools/mpi/mpibind/hwloc2.8-fluxcore0.43/share/mpibind
```

Once `FLUX_SHELL_RC_PATH` is set in your environment, mpibind will be
active by default. One thing to watch out for (on non Livermore
Computing systems) is to turn off mpibind
when allocating nodes, e.g., `flux alloc -N<nnodes> -o
mpibind=off`. In other words, you want to use mpibind only on your
Flux `run` commands.


## If nothing else, ask for the whole node!

mpibind has been running on production systems with Slurm for many
years. There are distinct differences between Flux and Slurm,
particularly, how and which resources are given to user jobs by
default.

The best way to run mpibind with Flux, which resembles how mpibind works
with Slurm, is to always ask Flux for all of the resources of a node
(`--exclusive`) and let mpibind assign those resources to the
processes and threads in a job:

<details>
<summary>

```
$ flux run -N1 -n4 --exclusive ./mpi-tioga
```
</summary>

```
tioga18    Task   2/  4 running on 16 CPUs: 32-47
           Task   2/  4 has 2 GPUs: 0xd9 0xde
tioga18    Task   3/  4 running on 16 CPUs: 48-63
           Task   3/  4 has 2 GPUs: 0xc1 0xc6
tioga18    Task   1/  4 running on 16 CPUs: 16-31
           Task   1/  4 has 2 GPUs: 0xc9 0xce
tioga18    Task   0/  4 running on 16 CPUs: 0-15
           Task   0/  4 has 2 GPUs: 0xd1 0xd6

```

<img src="../figures/tioga/figureI.png" width="550"/>


</details>


In this example, each task is running on a NUMA domain: 16 cores and 2
GPUs. Importantly, the 2 GPUs of a given task are local to the given
CPUs!

By default, mpibind assigns one hardware thread
(CPU) per core. This is by design allowing other hardware threads to
process system tasks and reduce their impact (OS noise) on application
threads. The number of CPUs per core available for application work is
a runtime parameter that can be changed, as we will show you later in
this document.


Let's now contrast this example with the behavior when we don't ask
for the whole node:

<details>
<summary>

```
$ flux run -N1 -n4 ./mpi-tioga
```
</summary>

```
tioga18    Task   2/  4 running on 1 CPUs: 62
           Task   2/  4 has 1 GPUs: 0xc6
tioga18    Task   1/  4 running on 1 CPUs: 61
           Task   1/  4 has 1 GPUs: 0xc6
tioga18    Task   3/  4 running on 1 CPUs: 63
           Task   3/  4 has 1 GPUs: 0xc6
tioga18    Task   0/  4 running on 1 CPUs: 60
           Task   0/  4 has 1 GPUs: 0xc6

```

<img src="../figures/tioga/figureJ.png" width="550"/>


</details>

Flux assigns four cores (60-63) to the job. mpibind uses those
resources to assign CPUs and GPUs to each task. In this case, each
task gets a CPU (remember one CPU per core by default) and a GPU. The
reason each task gets the same GPU is because GPU `0xc6` is the only
GPU local to cores 60-63.

In general, when a user asks for a subset of the resources of a node,
e.g., using `-c<ncores-per-task>`, mpibind is limited to those
resources allocated by Flux and, thus, it limits how much it can
leverage locality of the hardware topology!


The `mpi-tioga` program is nice, because it tells me where everything
is running. However, if you want to know what the mapping is for a
given program, then use the verbosity option:


<details>
<summary>

```
$ flux run -N1 -n4 --exclusive -o mpibind=verbose:1 <prog>
```
</summary>

```
0.450s: flux-shell[0]: mpibind:
mpibind: task  0 nths 16 gpus 4,5 cpus 0-15
mpibind: task  1 nths 16 gpus 2,3 cpus 16-31
mpibind: task  2 nths 16 gpus 6,7 cpus 32-47
mpibind: task  3 nths 16 gpus 0,1 cpus 48-63
...
```
</details>

The verbosity option shows the CPUs and GPUs each task is
using. In addition, it shows the number of threads (`nths`) that
would be used if this program has OpenMP in it.

For users that want full control over how their tasks, threads, and
GPU kernels are placed, then you can turn mpibind off and do your own
thing:


<details>
<summary>

```
$ flux run -N1 -n4 --exclusive -o mpibind=off ./mpi-tioga
```
</summary>

```
tioga18    Task   1/  4 running on 128 CPUs: 0-127
           Task   1/  4 has 8 GPUs: 0xc1 0xc6 0xc9 0xce 0xd1 0xd6 0xd9 0xde
tioga18    Task   2/  4 running on 128 CPUs: 0-127
           Task   2/  4 has 8 GPUs: 0xc1 0xc6 0xc9 0xce 0xd1 0xd6 0xd9 0xde
tioga18    Task   0/  4 running on 128 CPUs: 0-127
           Task   0/  4 has 8 GPUs: 0xc1 0xc6 0xc9 0xce 0xd1 0xd6 0xd9 0xde
tioga18    Task   3/  4 running on 128 CPUs: 0-127
           Task   3/  4 has 8 GPUs: 0xc1 0xc6 0xc9 0xce 0xd1 0xd6 0xd9 0xde
```

<img src="../figures/tioga/figureK.png" width="550"/>


</details>

You can also apply Flux affinity on the same command where you
disabled mpibind:

<details>
<summary>

```
$ flux run -N1 -n4 --exclusive -o mpibind=off -o gpu-affinity=per-task ./mpi-tioga
```
</summary>

```
tioga18    Task   2/  4 running on 128 CPUs: 0-127
           Task   2/  4 has 2 GPUs: 0xd1 0xd6
tioga18    Task   3/  4 running on 128 CPUs: 0-127
           Task   3/  4 has 2 GPUs: 0xd9 0xde
tioga18    Task   0/  4 running on 128 CPUs: 0-127
           Task   0/  4 has 2 GPUs: 0xc1 0xc6
tioga18    Task   1/  4 running on 128 CPUs: 0-127
           Task   1/  4 has 2 GPUs: 0xc9 0xce

```

<img src="../figures/tioga/figureL.png" width="550"/>


</details>



## Remote memory accesses is a *no, no*, but...

One of the design principles of mpibind is to avoid remote memory
accesses. Remote accesses can happen when a process or task spans more
than one NUMA domain: The task can be running on socket/package A
(with local memory A) and accessing memory associated with
socket/package B (with local memory B). This results in longer latency
fetching data from main memory. Therefore, mpibind limits the scope of
a task to the scope associated with a NUMA domain and no
more. Furthermore, mpibind will maximize the amount of cache per
task.

For example, for an 8-task job, each task gets a full L3
cache and its associated resources (one NUMA domain, one GPU, and 8
cores):

<details>
<summary>

```
$ flux run -N1 -n8 --exclusive ./mpi-tioga
```
</summary>

```
tioga18    Task   4/  8 running on 8 CPUs: 32-39
           Task   4/  8 has 1 GPUs: 0xd9
tioga18    Task   1/  8 running on 8 CPUs: 8-15
           Task   1/  8 has 1 GPUs: 0xd6
tioga18    Task   5/  8 running on 8 CPUs: 40-47
           Task   5/  8 has 1 GPUs: 0xde
tioga18    Task   3/  8 running on 8 CPUs: 24-31
           Task   3/  8 has 1 GPUs: 0xce
tioga18    Task   2/  8 running on 8 CPUs: 16-23
           Task   2/  8 has 1 GPUs: 0xc9
tioga18    Task   7/  8 running on 8 CPUs: 56-63
           Task   7/  8 has 1 GPUs: 0xc6
tioga18    Task   0/  8 running on 8 CPUs: 0-7
           Task   0/  8 has 1 GPUs: 0xd1
tioga18    Task   6/  8 running on 8 CPUs: 48-55
           Task   6/  8 has 1 GPUs: 0xc1
```
</details>


Since we have 4 NUMA domains, the assignment of processes to NUMA
domains is clear: Each task gets one domain. Moreover, 2 tasks share the
same NUMA domain.

What happens when we have more NUMA domains than tasks? The
nominal behavior is to assign a single NUMA to each task:

<details>
<summary>

```
$ flux run -N1 -n2 --exclusive -o mpibind=greedy:0 ./mpi-tioga
```
</summary>

```
tioga18    Task   1/  2 running on 16 CPUs: 16-31
           Task   1/  2 has 2 GPUs: 0xc9 0xce
tioga18    Task   0/  2 running on 16 CPUs: 0-15
           Task   0/  2 has 2 GPUs: 0xd1 0xd6
```

<img src="../figures/tioga/figureM.png" width="550"/>


</details>


No remote memory accesses: Good! At the same time, the resources
associated with NUMA domains 2 and 3 are left idle, because a task
cannot span more than one NUMA domain. That's where the`greedy` option
of mpibind comes in.

There are times when users want to launch fewer tasks than NUMA
domains, and still they would like to access all of the
resources of a node. Most likely (hopefully), users will be managing the
underlying resources themselves through other runtimes, perhaps
Python.

To account for this use case, mpibind provides the `greedy`
option.
   * When `greedy` is `off` mpibind follows the *no remote memory accesses*
rule.
   * When `greedy` is `on` and the number of tasks is less than the
number of NUMA domains, mpibind will distribute the resouces evenly
among the tasks: A task will undoubtely get more than one NUMA
domain.


Let's see what happens when `greedy` is `on`:


<details>
<summary>

```
$ flux run -N1 -n2 --exclusive -o mpibind=greedy:1 ./mpi-tioga
```
</summary>

```
tioga18    Task   0/  2 running on 64 CPUs: 0-31,64-95
           Task   0/  2 has 4 GPUs: 0xd1 0xd6 0xc9 0xce
tioga18    Task   1/  2 running on 64 CPUs: 32-63,96-127
           Task   1/  2 has 4 GPUs: 0xd9 0xde 0xc1 0xc6
```

<img src="../figures/tioga/figureN.png" width="550"/>


</details>


As expected, each task gets all the resources associated with 2 NUMA
domains.

By default `greedy` is `on` on Tioga. That's the reason we had to turn
off `greedy` above to get mpibind's nominal behavior when the number
of tasks is less than the number of NUMA domains.



## I want more (hardware threads)!

As mentioned earlier, mpibind, by default, uses only one hardware
thread per core to allow OS services to run on the other hardware
threads (on an SMT architecture) and reduce their impact on
application threads.

In some situations, users may benefit from exploiting all of the
hardware threads available. To enable this use case, mpibind provides
the `smt` option. Let's see how it works.

Tioga is an SMT-2 architecture. The first hardware threads from all cores
are numbered 0-63, while the second threads are numbered 64-127. The
following example shows mpibind is using only one hardware thread per core.

<details>
<summary>

```
$ flux run -N1 -n8 --exclusive ./mpi-tioga
```
</summary>

```
tioga18    Task   4/  8 running on 8 CPUs: 32-39
           Task   4/  8 has 1 GPUs: 0xd9
tioga18    Task   1/  8 running on 8 CPUs: 8-15
           Task   1/  8 has 1 GPUs: 0xd6
tioga18    Task   5/  8 running on 8 CPUs: 40-47
           Task   5/  8 has 1 GPUs: 0xde
tioga18    Task   3/  8 running on 8 CPUs: 24-31
           Task   3/  8 has 1 GPUs: 0xce
tioga18    Task   2/  8 running on 8 CPUs: 16-23
           Task   2/  8 has 1 GPUs: 0xc9
tioga18    Task   7/  8 running on 8 CPUs: 56-63
           Task   7/  8 has 1 GPUs: 0xc6
tioga18    Task   0/  8 running on 8 CPUs: 0-7
           Task   0/  8 has 1 GPUs: 0xd1
tioga18    Task   6/  8 running on 8 CPUs: 48-55
           Task   6/  8 has 1 GPUs: 0xc1
```
</details>

As expected only CPUs 0-63 are used.


Let's use the `smt` option to use two hardware threads. Since this is an
SMT-2 architecture, we only have two options: 1 or 2 threads.


<details>
<summary>

```
$ flux run -N1 -n8 --exclusive -o mpibind=smt:2 ./mpi-tioga
```
</summary>

```
$ flux run -N1 -n8 --exclusive -o mpibind=smt:2 ./mpi-tioga
tioga18    Task   5/  8 running on 16 CPUs: 40-47,104-111
           Task   5/  8 has 1 GPUs: 0xde
tioga18    Task   6/  8 running on 16 CPUs: 48-55,112-119
           Task   6/  8 has 1 GPUs: 0xc1
tioga18    Task   4/  8 running on 16 CPUs: 32-39,96-103
           Task   4/  8 has 1 GPUs: 0xd9
tioga18    Task   3/  8 running on 16 CPUs: 24-31,88-95
           Task   3/  8 has 1 GPUs: 0xce
tioga18    Task   2/  8 running on 16 CPUs: 16-23,80-87
           Task   2/  8 has 1 GPUs: 0xc9
tioga18    Task   1/  8 running on 16 CPUs: 8-15,72-79
           Task   1/  8 has 1 GPUs: 0xd6
tioga18    Task   7/  8 running on 16 CPUs: 56-63,120-127
           Task   7/  8 has 1 GPUs: 0xc6
tioga18    Task   0/  8 running on 16 CPUs: 0-7,64-71
           Task   0/  8 has 1 GPUs: 0xd1

```
</details>

Voilà, each task is using full cores!


## Let me drive (your OpenMP)

When using OpenMP in a hybrid program, users usually need to
determine the number of threads to run based on the underlying
hardware available to each process. While OpenMP runtimes have a
default, its value may or may not be sensible.

With mpibind, users are encouraged to **not** set the number of OpenMP
threads (`OMP_NUM_THREADS`). mpibind calculates this number for you
based on the resources that have been allocated to each process and
binds each OpenMP thread to the hardware. If a user sets the number of
threads, mpibind honors the request, but it may not be the optimal number. In other
words, do not set the number of OpenMP threads unless you know what
you are doing.

Let's review some examples starting with the default behavior in Flux
and moving to mpibind.


<details>
<summary>

```
$ flux run -N1 -n2 -o mpibind=off ./mpi+omp-tioga
```
</summary>

```
tioga18 Task   0/  2 Thread   0/  4 with  2 cpus: 62,126
tioga18 Task   0/  2 Thread   1/  4 with  2 cpus: 62,126
tioga18 Task   0/  2 Thread   2/  4 with  2 cpus: 63,127
tioga18 Task   0/  2 Thread   3/  4 with  2 cpus: 63,127
tioga18 Task   1/  2 Thread   0/  4 with  2 cpus: 62,126
tioga18 Task   1/  2 Thread   1/  4 with  2 cpus: 62,126
tioga18 Task   1/  2 Thread   2/  4 with  2 cpus: 63,127
tioga18 Task   1/  2 Thread   3/  4 with  2 cpus: 63,127
```
</details>

In this example, there are four OpenMP threads per task, because each
task is bound to 2 cores. Remember that by default Flux allocates one
core per task. Note that resources are oversubscribed!

To improve this case, let's use Flux's affinity:

<details>
<summary>

```
$ flux run -N1 -n2 -o mpibind=off -o cpu-affinity=per-task ./mpi+omp-tioga
```
</summary>

```
tioga18 Task   0/  2 Thread   0/  2 with  2 cpus: 62,126
tioga18 Task   0/  2 Thread   1/  2 with  2 cpus: 62,126
tioga18 Task   1/  2 Thread   0/  2 with  2 cpus: 63,127
tioga18 Task   1/  2 Thread   1/  2 with  2 cpus: 63,127
```
</details>

Much better, resource are not oversubscribed anymore: 2 OpenMP threads
and 1 core (2 CPUs) per process. However, the OS may migrate the two
threads of each process within the core's hardware threads.

Let's see what mpibind does:

<details>
<summary>

```
$ flux run -N1 -n2 ./mpi+omp-tioga
```
</summary>

```
tioga18 Task   0/  2 Thread   0/  1 with  1 cpus: 62
tioga18 Task   1/  2 Thread   0/  1 with  1 cpus: 63
```
</details>

mpibind uses 1 OpenMP thread per core, because of its noise mitigation
strategy. However, if a user wants to use both hardware threads, then
use the `smt` option as explained above:


<details>
<summary>

```
$ flux run -N1 -n2 -o mpibind=smt:2 ./mpi+omp-tioga
```
</summary>

```
tioga18 Task   0/  2 Thread   0/  2 with  1 cpus: 62
tioga18 Task   0/  2 Thread   1/  2 with  1 cpus: 126
tioga18 Task   1/  2 Thread   0/  2 with  1 cpus: 63
tioga18 Task   1/  2 Thread   1/  2 with  1 cpus: 127
```
</details>


Nice: The OS cannot migrate threads because each thread is bound to a
hardware thread.

To complete this section, let's see what happens when we ask for
GPUs. Let's start with Flux affinity again:

<details>
<summary>

```
$ flux run -N1 -n2 -g1 -o mpibind=off -o cpu-affinity=per-task -o gpu-affinity=per-task ./mpi+omp-tioga
```
</summary>

```
tioga18 Task   0/  2 Thread   0/  2 with  1 gpus: 0xd9
tioga18 Task   0/  2 Thread   0/  2 with  2 cpus: 62,126
tioga18 Task   0/  2 Thread   1/  2 with  1 gpus: 0xd9
tioga18 Task   0/  2 Thread   1/  2 with  2 cpus: 62,126
tioga18 Task   1/  2 Thread   0/  2 with  1 gpus: 0xde
tioga18 Task   1/  2 Thread   0/  2 with  2 cpus: 63,127
tioga18 Task   1/  2 Thread   1/  2 with  1 gpus: 0xde
tioga18 Task   1/  2 Thread   1/  2 with  2 cpus: 63,127
```
</details>

In this case, we asked Flux for 1 GPU per task. Indeed, each task got
a GPU. Note, however, that both GPUs are remote to cores 62 and 63. As
mentioned above, locality is not a primary consideration here.

Let's look at mpibind:

<details>
<summary>

```
$ flux run -N1 -n2 -g1 -o mpibind=smt:2 ./mpi+omp-tioga
```
</summary>

```
tioga18 Task   0/  2 Thread   0/  2 with  1 cpus: 62
tioga18 Task   0/  2 Thread   0/  2 with  1 gpus: 0xc6
tioga18 Task   0/  2 Thread   1/  2 with  1 cpus: 126
tioga18 Task   0/  2 Thread   1/  2 with  1 gpus: 0xc6
tioga18 Task   1/  2 Thread   0/  2 with  1 cpus: 63
tioga18 Task   1/  2 Thread   0/  2 with  1 gpus: 0xc6
tioga18 Task   1/  2 Thread   1/  2 with  1 cpus: 127
tioga18 Task   1/  2 Thread   1/  2 with  1 gpus: 0xc6
```
</details>


Interesting: We only got one GPU!

The reason is that for cores 62-63
the only local GPU is 0xc6. For mpibind, *locality is king*. As
mentioned above, to avoid this type of scenario, it is best to
allocate all the resources of a node and let mpibind manage them for
you.

Alternatively, if you want 2 GPUs, make sure to ask Flux for at least
8 cores per task. In this architecture there's a GPU per L3 complex,
which has 8 cores.

<details>
<summary>

```
$ flux run -N1 -n2 -c8 ./mpi+omp-tioga
```
</summary>

```
tioga18 Task   0/  2 Thread   0/  8 with  1 cpus: 48
tioga18 Task   0/  2 Thread   0/  8 with  1 gpus: 0xc1
tioga18 Task   0/  2 Thread   1/  8 with  1 cpus: 49
tioga18 Task   0/  2 Thread   1/  8 with  1 gpus: 0xc1
tioga18 Task   0/  2 Thread   2/  8 with  1 cpus: 50
tioga18 Task   0/  2 Thread   2/  8 with  1 gpus: 0xc1
tioga18 Task   0/  2 Thread   3/  8 with  1 cpus: 51
tioga18 Task   0/  2 Thread   3/  8 with  1 gpus: 0xc1
tioga18 Task   0/  2 Thread   4/  8 with  1 cpus: 52
tioga18 Task   0/  2 Thread   4/  8 with  1 gpus: 0xc1
tioga18 Task   0/  2 Thread   5/  8 with  1 cpus: 53
tioga18 Task   0/  2 Thread   5/  8 with  1 gpus: 0xc1
tioga18 Task   0/  2 Thread   6/  8 with  1 cpus: 54
tioga18 Task   0/  2 Thread   6/  8 with  1 gpus: 0xc1
tioga18 Task   0/  2 Thread   7/  8 with  1 cpus: 55
tioga18 Task   0/  2 Thread   7/  8 with  1 gpus: 0xc1
tioga18 Task   1/  2 Thread   0/  8 with  1 cpus: 56
tioga18 Task   1/  2 Thread   0/  8 with  1 gpus: 0xc6
tioga18 Task   1/  2 Thread   1/  8 with  1 cpus: 57
tioga18 Task   1/  2 Thread   1/  8 with  1 gpus: 0xc6
tioga18 Task   1/  2 Thread   2/  8 with  1 cpus: 58
tioga18 Task   1/  2 Thread   2/  8 with  1 gpus: 0xc6
tioga18 Task   1/  2 Thread   3/  8 with  1 cpus: 59
tioga18 Task   1/  2 Thread   3/  8 with  1 gpus: 0xc6
tioga18 Task   1/  2 Thread   4/  8 with  1 cpus: 60
tioga18 Task   1/  2 Thread   4/  8 with  1 gpus: 0xc6
tioga18 Task   1/  2 Thread   5/  8 with  1 cpus: 61
tioga18 Task   1/  2 Thread   5/  8 with  1 gpus: 0xc6
tioga18 Task   1/  2 Thread   6/  8 with  1 cpus: 62
tioga18 Task   1/  2 Thread   6/  8 with  1 gpus: 0xc6
tioga18 Task   1/  2 Thread   7/  8 with  1 cpus: 63
tioga18 Task   1/  2 Thread   7/  8 with  1 gpus: 0xc6
```
</details>

`0xc1` is the local GPU of cores `48-55`, while `0xc6` is the local GPU of
cores `56-63`!

## CPU-optimized or GPU-optimized mappings? 

By default mpibind provides a GPU-optimized mapping on systems with
GPUs. There may be cases, however, when users want a mapping that is
optimized for CPUs. For that mpibind provides the option
`gpu_optim:0|1`. Let's see how it works.

Let's say we want to launch a job with 9 tasks and 2 cores per
task. We can do this fairly easily with Flux native affinity:


<details>
<summary>

```
$ flux run -N1 -n9 -c2 -o mpibind=off -o cpu-affinity=per-task ./mpi-tioga
```
</summary>

```
tioga37    Task   0/  9 running on 4 CPUs: 46-47,110-111
tioga37    Task   1/  9 running on 4 CPUs: 48-49,112-113
tioga37    Task   2/  9 running on 4 CPUs: 50-51,114-115
tioga37    Task   3/  9 running on 4 CPUs: 52-53,116-117
tioga37    Task   4/  9 running on 4 CPUs: 54-55,118-119
tioga37    Task   5/  9 running on 4 CPUs: 56-57,120-121
tioga37    Task   6/  9 running on 4 CPUs: 58-59,122-123
tioga37    Task   7/  9 running on 4 CPUs: 60-61,124-125
tioga37    Task   8/  9 running on 4 CPUs: 62-63,126-127
           Task   0/  9 has 0 GPUs: 
           Task   1/  9 has 0 GPUs: 
           Task   2/  9 has 0 GPUs: 
           Task   3/  9 has 0 GPUs: 
           Task   4/  9 has 0 GPUs: 
           Task   5/  9 has 0 GPUs: 
           Task   6/  9 has 0 GPUs: 
           Task   7/  9 has 0 GPUs: 
           Task   8/  9 has 0 GPUs: 
```
</details>

Flux allocated Cores 46-47 from NUMA 2 and Cores 48-63 from NUMA
3. Then, it assigned, as requested, 2 cores per task. This is a fine
use of Flux affinity that does not require mpibind, but let's explore
how to achieve a similar behavior with mpibind. 

Let's run the same job with mpibind and see what we get:


<details>
<summary>

```
$ flux run -N1 -n9 -c2 ./mpi-tioga
```
</summary>

```
tioga37    Task   0/  9 running on 1 CPUs: 46
tioga37    Task   1/  9 running on 1 CPUs: 110
tioga37    Task   2/  9 running on 2 CPUs: 47,111
tioga37    Task   3/  9 running on 3 CPUs: 48-50
tioga37    Task   4/  9 running on 3 CPUs: 51-53
tioga37    Task   5/  9 running on 3 CPUs: 54-56
tioga37    Task   6/  9 running on 3 CPUs: 57-59
tioga37    Task   7/  9 running on 2 CPUs: 60-61
tioga37    Task   8/  9 running on 2 CPUs: 62-63
           Task   0/  9 has 1 GPUs: 0000:de 
           Task   1/  9 has 1 GPUs: 0000:de 
           Task   2/  9 has 1 GPUs: 0000:de 
           Task   3/  9 has 1 GPUs: 0000:c1 
           Task   4/  9 has 1 GPUs: 0000:c1 
           Task   5/  9 has 1 GPUs: 0000:c1 
           Task   6/  9 has 1 GPUs: 0000:c6 
           Task   7/  9 has 1 GPUs: 0000:c6 
           Task   8/  9 has 1 GPUs: 0000:c6 
```
</details>


At first, this mapping looks strange. To understand it, let's ignore
the CPU mapping and focus on the GPU mapping. There are 3 local
GPUs (0xde 0xc1 0xc6) associated with the allocated cores and the 9
tasks are perfectly distributed over these GPUs: 3 tasks per
GPU. Because this system has GPUs, mpibind 
provides a GPU-optimized mapping by default! 

Let's dig a little deeper. The cores allocated by Flux are
coming from NUMA 2 and NUMA 3. There is 1 local GPU associated to
Cores 46-47 on NUMA 2, while there are 2 local GPUs associated with
Cores 48-63 on NUMA 3. This results in 1/3 of the tasks (3) mapped to NUMA
2 and 2/3 of the tasks (6) mapped to NUMA 3. 

Now, let's tell mpibind that we want to prioritize the CPUs even
though this system has GPUs:


<details>
<summary>

```
$ flux run -N1 -n9 -c2 -o mpibind=gpu_optim:0 ./mpi-tioga
```
</summary>

```
tioga28    Task   0/  9 running on 2 CPUs: 46-47
tioga28    Task   1/  9 running on 2 CPUs: 48-49
tioga28    Task   2/  9 running on 2 CPUs: 50-51
tioga28    Task   3/  9 running on 2 CPUs: 52-53
tioga28    Task   4/  9 running on 2 CPUs: 54-55
tioga28    Task   5/  9 running on 2 CPUs: 56-57
tioga28    Task   6/  9 running on 2 CPUs: 58-59
tioga28    Task   7/  9 running on 2 CPUs: 60-61
tioga28    Task   8/  9 running on 2 CPUs: 62-63
           Task   0/  9 has 1 GPUs: 0000:de 
           Task   1/  9 has 1 GPUs: 0000:c1 
           Task   2/  9 has 1 GPUs: 0000:c1 
           Task   3/  9 has 1 GPUs: 0000:c1 
           Task   4/  9 has 1 GPUs: 0000:c1 
           Task   5/  9 has 1 GPUs: 0000:c6 
           Task   6/  9 has 1 GPUs: 0000:c6 
           Task   7/  9 has 1 GPUs: 0000:c6 
           Task   8/  9 has 1 GPUs: 0000:c6 
```
</details>

That's what we were looking for: One task assigned to two cores. If we
wanted both CPUs of each core, like Flux native affinity, we can do
that: 


<details>
<summary>

```
$ flux run -N1 -n9 -c2 -o mpibind=gpu_optim:0,smt:2 ./mpi-tioga 
```
</summary>

```
tioga28    Task   0/  9 running on 4 CPUs: 46-47,110-111
tioga28    Task   1/  9 running on 4 CPUs: 48-49,112-113
tioga28    Task   2/  9 running on 4 CPUs: 50-51,114-115
tioga28    Task   3/  9 running on 4 CPUs: 52-53,116-117
tioga28    Task   4/  9 running on 4 CPUs: 54-55,118-119
tioga28    Task   5/  9 running on 4 CPUs: 56-57,120-121
tioga28    Task   6/  9 running on 4 CPUs: 58-59,122-123
tioga28    Task   7/  9 running on 4 CPUs: 60-61,124-125
tioga28    Task   8/  9 running on 4 CPUs: 62-63,126-127
           Task   0/  9 has 1 GPUs: 0000:de 
           Task   1/  9 has 1 GPUs: 0000:c1 
           Task   2/  9 has 1 GPUs: 0000:c1 
           Task   3/  9 has 1 GPUs: 0000:c1 
           Task   4/  9 has 1 GPUs: 0000:c1 
           Task   5/  9 has 1 GPUs: 0000:c6 
           Task   6/  9 has 1 GPUs: 0000:c6 
           Task   7/  9 has 1 GPUs: 0000:c6 
           Task   8/  9 has 1 GPUs: 0000:c6 
```
</details>


One significant advantage of using mpibind over Flux native affinity
though is that mpibind provides access to the local GPUs! 



## How about system noise?

We will leverage the following environment variables for this section:

```
MPIBIND_RESTRICT_TYPE=cpu|mem
MPIBIND_RESTRICT=<list-of-integers>
```

*System noise*&#8212;any process, hardware or software, that delays an
application's execution and is not directly controlled by the
application&#8212;can be a significant source of performance
degradation. There are several techniques to mitigate system noise,
here we cover **Core Specialization**, but the same mechanism can be
used for **Thread Specialization**.

As its name implies, core specialization dedicates a number of cores
for the processing of system services. This scheme partitions the
available cores into two categories: *OS Cores* and *Application
Cores*. With `mpibind`, one can specify the application cores. Here
are a few examples.


Let's start with the default behavior:

<details>
<summary>

```
$ flux run -N1 -n4 --exclusive ./mpi-tioga
```
</summary>

```
tioga19    Task   0/  4 running on 16 CPUs: 0-15
tioga19    Task   1/  4 running on 16 CPUs: 16-31
tioga19    Task   2/  4 running on 16 CPUs: 32-47
tioga19    Task   3/  4 running on 16 CPUs: 48-63
           Task   0/  4 has 2 GPUs: 0000:d1 0000:d6 
           Task   1/  4 has 2 GPUs: 0000:c9 0000:ce 
           Task   2/  4 has 2 GPUs: 0000:d9 0000:de 
           Task   3/  4 has 2 GPUs: 0000:c1 0000:c6 
```
</details>

Let's assume 4 cores have been set aside for system services, one per NUMA domain:
Core 0, 16, 32, and 48.

Now, we tell mpibind which cores are the application cores (the
complement): Cores 1-15, 17-31, 33-47, and 49-63, which 
correspond to CPUs 1-15, 65-79, 17-31, 81-95, 33-47, 97-111, 49-63,
and 113-127 since Tioga has two hardware threads per core. 

<details>
<summary>

```
$ export MPIBIND_RESTRICT=1-15,65-79,17-31,81-95,33-47,97-111,49-63,113-127
$ flux run -N1 -n4 --exclusive ./mpi-tioga
```
</summary>

```
tioga19    Task   0/  4 running on 15 CPUs: 1-15
tioga19    Task   1/  4 running on 15 CPUs: 17-31
tioga19    Task   2/  4 running on 15 CPUs: 33-47
tioga19    Task   3/  4 running on 15 CPUs: 49-63
           Task   0/  4 has 2 GPUs: 0000:d1 0000:d6 
           Task   1/  4 has 2 GPUs: 0000:c9 0000:ce 
           Task   2/  4 has 2 GPUs: 0000:d9 0000:de 
           Task   3/  4 has 2 GPUs: 0000:c1 0000:c6 
```
</details>

Notice how the system cores are no longer used to schedule application
work.

If we want to use both hardware threads per core, we can still do
that:

<details>
<summary>

```
$ export MPIBIND_RESTRICT=1-15,65-79,17-31,81-95,33-47,97-111,49-63,113-127
$ flux run -N1 -n4 --exclusive -o mpibind=smt:2 ./mpi-tioga 
```
</summary>

```
tioga19    Task   0/  4 running on 30 CPUs: 1-15,65-79
tioga19    Task   1/  4 running on 30 CPUs: 17-31,81-95
tioga19    Task   2/  4 running on 30 CPUs: 33-47,97-111
tioga19    Task   3/  4 running on 30 CPUs: 49-63,113-127
           Task   0/  4 has 2 GPUs: 0000:d1 0000:d6 
           Task   1/  4 has 2 GPUs: 0000:c9 0000:ce 
           Task   2/  4 has 2 GPUs: 0000:d9 0000:de 
           Task   3/  4 has 2 GPUs: 0000:c1 0000:c6
```
</details>

Notice again that no system cores are used!

Let's now run our application on a single NUMA domain, say NUMA 1,
and its associated resources.

Instead of figuring out what CPUs are associated with NUMA 1 and
setting MPIBIND_RESTRICT to those CPUs, we can tell mpibind directly
we want the resources associated with a particular memory domain(s)
using `MPIBIND_RESTRICT_TYPE` as follows:

```
MPIBIND_RESTRICT_TYPE=mem
```

By default MPIBIND_RESTRICT_TYPE is set to `cpu`, that's why we did not
have to set this variable in the previous examples.

<details>
<summary>

```
$ export MPIBIND_RESTRICT_TYPE=mem
$ export MPIBIND_RESTRICT=1
$ flux run -N1 -n4 --exclusive ./mpi-tioga
```
</summary>

```
tioga19    Task   0/  4 running on 4 CPUs: 16-19
tioga19    Task   1/  4 running on 4 CPUs: 20-23
tioga19    Task   2/  4 running on 4 CPUs: 24-27
tioga19    Task   3/  4 running on 4 CPUs: 28-31
           Task   0/  4 has 1 GPUs: 0000:c9 
           Task   1/  4 has 1 GPUs: 0000:c9 
           Task   2/  4 has 1 GPUs: 0000:ce 
           Task   3/  4 has 1 GPUs: 0000:ce 
```
</details>


In this case only Cores 16-31 and GPUs 0xc9 0xce are used since these
are the local resources associated with NUMA domain 1. 


While core specialization works well to mitigate the impact of system
noise on applications, there may be cases where an application may want to
use *all* of the resources of a node.

Let's say Tioga has been configured by the system admins to use
the first core of every NUMA for system services. In that case the
output of our program would be as follows:

<details>
<summary>

```
$ flux run -N1 -n4 --exclusive ./mpi-tioga
```
</summary>

```
tioga19    Task   0/  4 running on 15 CPUs: 1-15
tioga19    Task   1/  4 running on 15 CPUs: 17-31
tioga19    Task   2/  4 running on 15 CPUs: 33-47
tioga19    Task   3/  4 running on 15 CPUs: 49-63
           Task   0/  4 has 2 GPUs: 0000:d1 0000:d6 
           Task   1/  4 has 2 GPUs: 0000:c9 0000:ce 
           Task   2/  4 has 2 GPUs: 0000:d9 0000:de 
           Task   3/  4 has 2 GPUs: 0000:c1 0000:c6 
```
</details>

Notice how the system cores are not used for application
processing.

To use all of the resources, including the system cores, we simply
unset MPIBIND_RESTRICT:


<details>
<summary>

```
$ unset MPIBIND_RESTRICT
$ flux run -N1 -n4 --exclusive ./mpi-tioga
```
</summary>

```
tioga19    Task   0/  4 running on 16 CPUs: 0-15
tioga19    Task   1/  4 running on 16 CPUs: 16-31
tioga19    Task   2/  4 running on 16 CPUs: 32-47
tioga19    Task   3/  4 running on 16 CPUs: 48-63
           Task   0/  4 has 2 GPUs: 0000:d1 0000:d6 
           Task   1/  4 has 2 GPUs: 0000:c9 0000:ce 
           Task   2/  4 has 2 GPUs: 0000:d9 0000:de 
           Task   3/  4 has 2 GPUs: 0000:c1 0000:c6 
```
</details>

Voilà. 


Finally, while `mpibind` can control what resources to use for
applications, it does not migrate system services to run on the OS
cores: It is up to the system admins to do that. The idea is that
system admins would set up the system services to run on specific
cores and then set mpibind appropriately using the environment
variables specified here. 


## References

* [GitHub repo](https://github.com/LLNL/mpibind)
* [Affinity programs](https://github.com/LLNL/mpibind/tree/master/affinity)






<!--

<details>
<summary>

```
```
</summary>

```
```
</details>
-->
