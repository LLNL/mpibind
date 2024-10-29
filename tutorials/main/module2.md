# Module 2: Automatic affinity with mpibind

*Edgar A. León* and *Jane E. Herriman*<br>
Lawrence Livermore National Laboratory


## Table of contents

1. Making sense of affinity: [Discovering the node architecture topology](module1.md)
2. Applying automatic affinity: mpibind
   1. [Learning objectives](#learning-objectives)
   1. [Definitions](#definitions)
   1. [Example architectures](#example-architectures)
   1. [Example mappings](#example-mappings)
   1. [Reporting affinity](#reporting-affinity)
   1. [A memory-driven mapping algorithm for heterogeneous systems](#a-memory-driven-mapping-algorithm-for-heterogeneous-systems)
   1. [Key concept 1: Require minimal user input](#key-concept-1-require-minimal-user-input)
   1. [Key concept 2: Avoid multiple NUMA domains per worker](#key-concept-2-avoid-multiple-numa-domains-per-worker)
   1. [Key concept 3: Maximize cache per worker](#key-concept-3-maximize-cache-per-worker)
   1. [Key concept 4: Leverage GPU locality](#key-concept-4-leverage-gpu-locality)
   1. [Key concept 5: Leverage CPU locality](#key-concept-5-leverage-cpu-locality)
   1. [Key concept 6: Leverage SMT support](#key-concept-6-leverage-smt-support)
   1. [Interfacing with mpibind](#interfacing-with-mpibind)
   1. [Hands-on exercises](#hands-on-exercises)
   1. [References](#references)
3. Exerting resource manger affinity: [Process affinity with Slurm](module3.md)
4. Exerting thread affinity: [OpenMP](module4.md)
5. Putting it all together: [Adding in GPUs](module5.md)


## Learning objectives

* Understand the concepts of affinity, binding, and mapping.
* Identify the resources available to the processes and threads of a
hybrid program.
* Understand the underlying principles of automatic affinity with
 mpibind, particularly the principle of locality. 
* Run an application with and without mpibind.
* Tune mpibind for different resource constraints such as memory, GPUs, or CPUs.


## Definitions

**Mapping**: A function from the worker set (our set of tasks and/or threads) to the hardware resource set. 

**Affinity**: Policies for how computational tasks map to hardware.

**Binding**: Mechanism for implementing and changing the mappings of a given affinity policy. 

*Mappings*, *affinity*, and *bindings* are key and interrelated concepts in this tutorial, and so we want to define and distinguish between them. When we know a program's hardware mapping, we know which hardware resources (for example, which cores and which caches) are available to each of the workers executing the program. The affinity is a policy that informs the mapping used for a particular combination of hardware and workers, and then workers are bound to hardware resources as defined by the mapping. 

For example, an affinity policy asking processes to spread out may result in a mapping where Process 1 is assigned to Core 1 and Process 2 maps to Core 16. After binding to Core 16, Process 2 can start performing computational work there.

Since we're just getting started, these concepts may still feel abstract, but our hope is that after seeing some concrete examples in this module, these definitions will start to feel more straightforward.

## Example architectures

The topologies of a few example architectures are summarized and diagramed [here](../common/archs.md).

We'll be using these machines (`Pascal`, `Corona`, and `Lassen`) in examples throughout the coming modules!

<!-- EL: Previous table might be simpler to modify
<style type="text/css">
.tg  {border-collapse:collapse;border-spacing:0;}
.tg td{border-color:black;border-style:solid;border-width:1px;font-family:Arial, sans-serif;font-size:14px;
  overflow:hidden;padding:10px 5px;word-break:normal;}
.tg th{border-color:black;border-style:solid;border-width:1px;font-family:Arial, sans-serif;font-size:14px;
  font-weight:normal;overflow:hidden;padding:10px 5px;word-break:normal;}
.tg .tg-c3ow{border-color:inherit;text-align:center;vertical-align:top}
.tg .tg-7btt{border-color:inherit;font-weight:bold;text-align:center;vertical-align:top}
</style>
<table class="tg">
<thead>
  <tr>
    <th class="tg-7btt">Ruby</th>
    <th class="tg-7btt">Mammoth</th>
    <th class="tg-7btt">Corona</th>
  </tr>
</thead>
<tbody>
  <tr>
    <td class="tg-c3ow">2 Intel Xeon processors<br><br>1 NUMA domain with 28 cores per processor<br><br>2 hardware threads per core<br><br>   </td>
    <td class="tg-c3ow">2 AMD Rome processors<br><br>1 NUMA domain with 64 cores per processor (NPS1)<br><br>2 hardware threads per core<br> <br>    </td>
    <td class="tg-c3ow">2 AMD Rome processors<br><br>1 NUMA domain with 24 cores per processor (NPS1)<br><br>2 hardware threads per core<br><br>4 AMD MI50 GPUs per processor</td>
  </tr>
</tbody>
</table>
-->

<!-- Commenting out since Gitlab does not display PDFs
<object data="../hwloc/ruby.pdf" type="application/pdf" width="800px" height="800px">
</object>

<object data="../hwloc/mammoth.pdf" type="application/pdf" width="800px" height="800px">
</object>

<object data="../hwloc/corona.pdf" type="application/pdf" width="800px" height="800px">
</object>
-->

## Example mappings

#### Example 1: MPI mappings
##### Example 1.A
Suppose we have an affinity policy that specifies **one MPI task per `Core` on `Pascal`**. We might get the following mapping:

* <i>Task 0: PU 0,36</i>
* <i>Task 1: PU 1,37</i>
* <i>...</i>
* <i>Task 18: PU 18,54</i>
* <i>... </i>
* <i>Task 35: PU 35,71</i>

Note that each core on `Pascal` has two processing units (PUs), so every task assigned to a single core is assigned to two PUs. Each `Pascal` node has 2 processors each with 18 cores for a total of 36 cores per node, so we can have a max of 36 tasks on a single node given the "one task per core" affinity policy.

##### Example 1.B
Next, suppose we had an affinity policy with **one MPI task per `L3` on `Corona`**. In the image depicting `Corona`'s topology above, we see that each `L3` cache on `Corona` has 3 cores. Since one `Corona` node has 48 cores, we'd expect to see 16 tasks on a node with 3 cores per task, perhaps with the following mapping:

* <i>Task 0: PU 0-2,48-50</i>
* <i>Task 1: PU 3-5,51-53</i>
* <i>...</i>
* <i>Task 8: PU 24-26,72-74</i>
* <i>...</i>
* <i>Task 15: PU 45-47,93-95</i>

The auxiliary commands

```
lstopo -i pascal.xml -p --only core 
lstopo -i corona.xml -p --only l3 
``` 

can be used to reproduce these mappings for yourself using the provied `.xml` files that describe the topology of `Pascal` and `Corona`.


#### Example 2: OpenMP mappings

Consider an affinity policy with **one OpenMP thread per `Core` on `Lassen`**:

* <i>Thread 0: PU 8</i>
* <i>Thread 1: PU 12</i>
* <i>...</i>
* <i>Thread 19: PU 92</i>
* <i>...</i>
* <i>Thread 39: PU 172</i>

and how this compares to an affinity policy with **one OpenMP thread per `PU` on `Lassen`**:

* <i>Thread 0: PU 8</i>
* <i>Thread 1: PU 9</i>
* <i>Thread 2: PU 10</i>
* <i>Thread 3: PU 11</i>
* <i>...</i>
* <i>Thread 78: PU 86</i>
* <i>Thread 79: PU 87</i>
* <i>...</i>
* <i>Thread 80: PU 96</i>
* <i>Thread 81: PU 97</i>
* <i>...</i>
* <i>Thread 159: PU 174</i>
* <i>Thread 160: PU 175</i>

Auxiliary commands

```
lstopo -i lassen.xml -p --only core  
lstopo -i lassen.xml -p --only pu  
```

#### Example 3: GPU mappings
##### Example 3.A
On `Corona`, consider an affinity policy with **one task per GPU**:

* <i>Task 0: rsmi0</i>
* <i>Task 1: rsmi1</i>
* <i>... </i>
* <i>Task 6: rsmi6</i>
* <i>Task 7: rsmi7</i>

##### Example 3.B
For an affinity policy with **one thread per GPU** on `Corona`, observe how many tasks we see:

* <i>Task 0, thread 0: rsmi0</i>
* <i>Task 0, thread 1: rsmi1</i>
* <i>Task 0, thread 2: rsmi2</i>
* <i>Task 0, thread 3: rsmi3</i>
* <i>Task 1, thread 0: rsmi4</i>
* <i>Task 1, thread 1: rsmi5</i>
* <i>Task 1, thread 2: rsmi6</i>
* <i>Task 1, thread 3: rsmi7</i>

We see that our affinity policy of one thread per GPU gave us 8 threads, each assigned to one of two tasks. Why do you think **two** tasks were created?

Auxiliary commands
```
lstopo -i corona.xml -p | grep -i -e numa -e gpu 
```

#### Comprehension question 1

Match the following:

“Task 8 —> PUs 24-26 and 72-74”

“Policy for one openMP thread per L3 on Corona”

**A)** Affinity

**B)** Mapping

<details>
<summary>
Answer
</summary>

B. Mapping: “Task 8 —> PUs 24-26 and 72-74”

A. Affinity: “Policy for one openMP thread per L3 on Corona”
</details>



## Reporting affinity

mpibind provides programs to report the mapping of each process and thread
to the CPUs and GPUs of a node. There are three variants:

* MPI: `mpi`
* OpenMP: `omp`
* MPI+OpenMP: `mpi+omp`

Usage is straightforward. Use the `-v` option for verbose GPU output and
the `-h` option for help.

The examples below are already using mpibind, but we will talk about that a little later.  

##### Example 5

`Corona` has 48 cores and 8 GPUs split across 2 processors. A program run with 4 MPI tasks gets 12 CPUs and 2 GPUs per task.

<details>
<summary>

```
$ srun -n4 ./mpi
```

</summary>

```
corona236  Task   0/  4 running on 12 CPUs: 0-11
           Task   0/  4 has 2 GPUs: 0x63 0x43
corona236  Task   1/  4 running on 12 CPUs: 12-23
           Task   1/  4 has 2 GPUs: 0x3 0x27 
corona236  Task   2/  4 running on 12 CPUs: 24-35
           Task   2/  4 has 2 GPUs: 0xe3 0xc3 
corona236  Task   3/  4 running on 12 CPUs: 36-47
           Task   3/  4 has 2 GPUs: 0x83 0xa3 
```

</details>

When we run with the `-v` flag to get "verbose" output, we see more information about the GPUs assigned to each task.

<details>
<summary>

```
$ srun -n4 ./mpi -v
```

</summary>

```
corona236  Task   0/  4 running on 12 CPUs: 0-11
           Task   0/  4 has 2 GPUs: 0x63 0x43 
	Default device: 0x63
	0x63: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
	0x43: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
corona236  Task   1/  4 running on 12 CPUs: 12-23
           Task   1/  4 has 2 GPUs: 0x3 0x27 
	Default device: 0x3
	0x03: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
	0x27: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
corona236  Task   2/  4 running on 12 CPUs: 24-35
           Task   2/  4 has 2 GPUs: 0xe3 0xc3 
	Default device: 0xe3
	0xe3: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
	0xc3: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
corona236  Task   3/  4 running on 12 CPUs: 36-47
           Task   3/  4 has 2 GPUs: 0x83 0xa3 
	Default device: 0x83
	0x83: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
	0xa3: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
```
</details>

##### Example 6

Still on `Corona`, let's look at the bindings we see when we run a program with 2 processes while `OMP_NUM_THREADS=4`. This means we'll create 4 OpenMP threads *per process*. Note that since we're not using MPI here, each process is independent.

We see below that, with 4 threads and 4 GPUs per process, *each thread gets its own GPU*:

<details>
<summary>

```
$ OMP_NUM_THREADS=4 srun -n2 ./omp
```

</summary>

```
Process running on 8 CPUs: 0,3,6,9,12,15,18,21
Process has 4 GPUs: 0x63 0x43 0x3 0x27 
	Default device: 0x63
	0x63: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
	0x43: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
	0x03: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
	0x27: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
Thread   0/  4 running on 1 CPUs: 0
Thread   0/  4 assigned to GPU: 0x63
Thread   1/  4 running on 1 CPUs: 6
Thread   1/  4 assigned to GPU: 0x43
Thread   2/  4 running on 1 CPUs: 12
Thread   2/  4 assigned to GPU: 0x3
Thread   3/  4 running on 1 CPUs: 18
Thread   3/  4 assigned to GPU: 0x27

Process running on 8 CPUs: 24,27,30,33,36,39,42,45
Process has 4 GPUs: 0xe3 0xc3 0x83 0xa3 
	Default device: 0xe3
	0xe3: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
	0xc3: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
	0x83: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
	0xa3: , 31 GB Mem, 60 Multiprocessors, 1.725 GHZ, 9.0 CC
Thread   0/  4 running on 1 CPUs: 24
Thread   0/  4 assigned to GPU: 0xe3
Thread   1/  4 running on 1 CPUs: 30
Thread   1/  4 assigned to GPU: 0xc3
Thread   2/  4 running on 1 CPUs: 36
Thread   2/  4 assigned to GPU: 0x83
Thread   3/  4 running on 1 CPUs: 42
Thread   3/  4 assigned to GPU: 0xa3  
```

</details>

**Auxiliary links**
```
https://github.com/LLNL/mpibind/tree/master/affinity
```




## A memory-driven mapping algorithm for heterogeneous systems

mpibind is a memory-driven algorithm to map parallel hybrid
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


||
|:--:|
|![Lassen](../figures/sierra.png ":Lassen")|


<!--
## Demonstration of key concepts

* Minimal user input: Number of tasks (optionally specify number of
 threads). No topology info required, e.g., CPU IDs, which are not
 portable.  
* Maximize cache per worker, e.g., L3.
* Don't spill NUMA domains
* MPIBIND=v 
* Show slurm command line
--> 

## Key concept 1: Require minimal user input

`mpibind` has been designed to distribute workers reasonably given minimal user input.

For example, if you simply run a program with a specified number tasks using `mpibind`, those tasks will be distributed across the node in a way that allocates (to best approximation) equal resources to each of those tasks. For example, here is how 4 tasks are mapped to a `Pascal` node. Note the number of CPUs per task and the number of tasks per GPU:

<details>
<summary>

```
pascal30$ srun -n4 ./mpi
```

</summary>

```
pascal30   Task   0/  4 running on 9 CPUs: 0-8
           Task   0/  4 has 1 GPUs: 0x4
pascal30   Task   1/  4 running on 9 CPUs: 9-17
           Task   1/  4 has 1 GPUs: 0x4
pascal30   Task   2/  4 running on 9 CPUs: 18-26
           Task   2/  4 has 1 GPUs: 0x7
pascal30   Task   3/  4 running on 9 CPUs: 27-35
           Task   3/  4 has 1 GPUs: 0x7
```

</details>

Next, consider what happens when we add in OpenMP threads and run the demo program `mpi+omp`. Below we see that if we continue with 4 tasks on a `Pascal` node, 9 threads per task are created. This results in a total of 36 threads -- one per core on `Pascal`:

<details>
<summary>

```
pascal30$ srun -n4 ./mpi+omp
```

</summary>

```
pascal30 Task   0/  4 Thread   0/  9 with  1 cpus: 0
pascal30 Task   0/  4 Thread   0/  9 with  1 gpus: 0x4
pascal30 Task   0/  4 Thread   3/  9 with  1 cpus: 3
pascal30 Task   0/  4 Thread   3/  9 with  1 gpus: 0x4
pascal30 Task   0/  4 Thread   8/  9 with  1 cpus: 8
pascal30 Task   0/  4 Thread   8/  9 with  1 gpus: 0x4
pascal30 Task   0/  4 Thread   2/  9 with  1 cpus: 2
pascal30 Task   0/  4 Thread   2/  9 with  1 gpus: 0x4
pascal30 Task   0/  4 Thread   4/  9 with  1 cpus: 4
pascal30 Task   0/  4 Thread   4/  9 with  1 gpus: 0x4
pascal30 Task   0/  4 Thread   1/  9 with  1 cpus: 1
pascal30 Task   0/  4 Thread   1/  9 with  1 gpus: 0x4
pascal30 Task   0/  4 Thread   5/  9 with  1 cpus: 5
pascal30 Task   0/  4 Thread   5/  9 with  1 gpus: 0x4
pascal30 Task   0/  4 Thread   6/  9 with  1 cpus: 6
pascal30 Task   0/  4 Thread   6/  9 with  1 gpus: 0x4
pascal30 Task   0/  4 Thread   7/  9 with  1 cpus: 7
pascal30 Task   0/  4 Thread   7/  9 with  1 gpus: 0x4
pascal30 Task   2/  4 Thread   3/  9 with  1 cpus: 21
pascal30 Task   2/  4 Thread   3/  9 with  1 gpus: 0x7
pascal30 Task   2/  4 Thread   0/  9 with  1 cpus: 18
pascal30 Task   2/  4 Thread   0/  9 with  1 gpus: 0x7
pascal30 Task   2/  4 Thread   2/  9 with  1 cpus: 20
pascal30 Task   2/  4 Thread   2/  9 with  1 gpus: 0x7
pascal30 Task   2/  4 Thread   4/  9 with  1 cpus: 22
pascal30 Task   2/  4 Thread   4/  9 with  1 gpus: 0x7
pascal30 Task   2/  4 Thread   1/  9 with  1 cpus: 19
pascal30 Task   2/  4 Thread   1/  9 with  1 gpus: 0x7
pascal30 Task   2/  4 Thread   8/  9 with  1 cpus: 26
pascal30 Task   2/  4 Thread   8/  9 with  1 gpus: 0x7
pascal30 Task   2/  4 Thread   5/  9 with  1 cpus: 23
pascal30 Task   2/  4 Thread   5/  9 with  1 gpus: 0x7
pascal30 Task   2/  4 Thread   7/  9 with  1 cpus: 25
pascal30 Task   2/  4 Thread   7/  9 with  1 gpus: 0x7
pascal30 Task   2/  4 Thread   6/  9 with  1 cpus: 24
pascal30 Task   2/  4 Thread   6/  9 with  1 gpus: 0x7
pascal30 Task   3/  4 Thread   0/  9 with  1 cpus: 27
pascal30 Task   3/  4 Thread   0/  9 with  1 gpus: 0x7
pascal30 Task   3/  4 Thread   3/  9 with  1 cpus: 30
pascal30 Task   3/  4 Thread   3/  9 with  1 gpus: 0x7
pascal30 Task   3/  4 Thread   1/  9 with  1 cpus: 28
pascal30 Task   3/  4 Thread   1/  9 with  1 gpus: 0x7
pascal30 Task   3/  4 Thread   2/  9 with  1 cpus: 29
pascal30 Task   3/  4 Thread   2/  9 with  1 gpus: 0x7
pascal30 Task   3/  4 Thread   4/  9 with  1 cpus: 31
pascal30 Task   3/  4 Thread   4/  9 with  1 gpus: 0x7
pascal30 Task   3/  4 Thread   8/  9 with  1 cpus: 35
pascal30 Task   3/  4 Thread   8/  9 with  1 gpus: 0x7
pascal30 Task   3/  4 Thread   5/  9 with  1 cpus: 32
pascal30 Task   3/  4 Thread   5/  9 with  1 gpus: 0x7
pascal30 Task   3/  4 Thread   6/  9 with  1 cpus: 33
pascal30 Task   3/  4 Thread   6/  9 with  1 gpus: 0x7
pascal30 Task   3/  4 Thread   7/  9 with  1 cpus: 34
pascal30 Task   3/  4 Thread   7/  9 with  1 gpus: 0x7
pascal30 Task   1/  4 Thread   0/  9 with  1 cpus: 9
pascal30 Task   1/  4 Thread   0/  9 with  1 gpus: 0x4
pascal30 Task   1/  4 Thread   3/  9 with  1 cpus: 12
pascal30 Task   1/  4 Thread   3/  9 with  1 gpus: 0x4
pascal30 Task   1/  4 Thread   2/  9 with  1 cpus: 11
pascal30 Task   1/  4 Thread   2/  9 with  1 gpus: 0x4
pascal30 Task   1/  4 Thread   8/  9 with  1 cpus: 17
pascal30 Task   1/  4 Thread   8/  9 with  1 gpus: 0x4
pascal30 Task   1/  4 Thread   4/  9 with  1 cpus: 13
pascal30 Task   1/  4 Thread   4/  9 with  1 gpus: 0x4
pascal30 Task   1/  4 Thread   6/  9 with  1 cpus: 15
pascal30 Task   1/  4 Thread   6/  9 with  1 gpus: 0x4
pascal30 Task   1/  4 Thread   1/  9 with  1 cpus: 10
pascal30 Task   1/  4 Thread   1/  9 with  1 gpus: 0x4
pascal30 Task   1/  4 Thread   7/  9 with  1 cpus: 16
pascal30 Task   1/  4 Thread   7/  9 with  1 gpus: 0x4
pascal30 Task   1/  4 Thread   5/  9 with  1 cpus: 14
pascal30 Task   1/  4 Thread   5/  9 with  1 gpus: 0x4
```     

</details>

Similarly, if we run an MPI + OpenMP program with 4 tasks on `Lassen`, where we have 40 cores per node, we'll see that a default of 10 threads per task will be generated. *By default, `mpibind` creates as many threads as cores.*

<details>
<summary>

```
lassen16$ lrun -T4 ./mpi+omp
```

</summary>

```
lassen16 Task   3/  4 Thread   0/ 10 with  1 cpus: 136
lassen16 Task   3/  4 Thread   0/ 10 with  1 gpus: 0x4
lassen16 Task   3/  4 Thread   3/ 10 with  1 cpus: 148
lassen16 Task   3/  4 Thread   3/ 10 with  1 gpus: 0x4
lassen16 Task   3/  4 Thread   8/ 10 with  1 cpus: 168
lassen16 Task   3/  4 Thread   8/ 10 with  1 gpus: 0x4
lassen16 Task   3/  4 Thread   1/ 10 with  1 cpus: 140
lassen16 Task   3/  4 Thread   1/ 10 with  1 gpus: 0x4
lassen16 Task   3/  4 Thread   5/ 10 with  1 cpus: 156
lassen16 Task   3/  4 Thread   5/ 10 with  1 gpus: 0x4
lassen16 Task   3/  4 Thread   6/ 10 with  1 cpus: 160
lassen16 Task   3/  4 Thread   6/ 10 with  1 gpus: 0x4
lassen16 Task   3/  4 Thread   4/ 10 with  1 cpus: 152
lassen16 Task   3/  4 Thread   4/ 10 with  1 gpus: 0x4
lassen16 Task   3/  4 Thread   7/ 10 with  1 cpus: 164
lassen16 Task   3/  4 Thread   7/ 10 with  1 gpus: 0x4
lassen16 Task   3/  4 Thread   2/ 10 with  1 cpus: 144
lassen16 Task   3/  4 Thread   2/ 10 with  1 gpus: 0x4
lassen16 Task   3/  4 Thread   9/ 10 with  1 cpus: 172
lassen16 Task   3/  4 Thread   9/ 10 with  1 gpus: 0x4
lassen16 Task   0/  4 Thread   0/ 10 with  1 cpus: 8
lassen16 Task   0/  4 Thread   0/ 10 with  1 gpus: 0x4
lassen16 Task   0/  4 Thread   4/ 10 with  1 cpus: 24
lassen16 Task   0/  4 Thread   4/ 10 with  1 gpus: 0x4
lassen16 Task   0/  4 Thread   5/ 10 with  1 cpus: 28
lassen16 Task   0/  4 Thread   5/ 10 with  1 gpus: 0x4
lassen16 Task   0/  4 Thread   3/ 10 with  1 cpus: 20
lassen16 Task   0/  4 Thread   3/ 10 with  1 gpus: 0x4
lassen16 Task   0/  4 Thread   2/ 10 with  1 cpus: 16
lassen16 Task   0/  4 Thread   2/ 10 with  1 gpus: 0x4
lassen16 Task   0/  4 Thread   7/ 10 with  1 cpus: 36
lassen16 Task   0/  4 Thread   7/ 10 with  1 gpus: 0x4
lassen16 Task   0/  4 Thread   6/ 10 with  1 cpus: 32
lassen16 Task   0/  4 Thread   6/ 10 with  1 gpus: 0x4
lassen16 Task   0/  4 Thread   1/ 10 with  1 cpus: 12
lassen16 Task   0/  4 Thread   1/ 10 with  1 gpus: 0x4
lassen16 Task   0/  4 Thread   8/ 10 with  1 cpus: 40
lassen16 Task   0/  4 Thread   8/ 10 with  1 gpus: 0x4
lassen16 Task   0/  4 Thread   9/ 10 with  1 cpus: 44
lassen16 Task   0/  4 Thread   9/ 10 with  1 gpus: 0x4
lassen16 Task   2/  4 Thread   2/ 10 with  1 cpus: 104
lassen16 Task   2/  4 Thread   2/ 10 with  1 gpus: 0x3
lassen16 Task   2/  4 Thread   3/ 10 with  1 cpus: 108
lassen16 Task   2/  4 Thread   3/ 10 with  1 gpus: 0x3
lassen16 Task   2/  4 Thread   4/ 10 with  1 cpus: 112
lassen16 Task   2/  4 Thread   4/ 10 with  1 gpus: 0x3
lassen16 Task   2/  4 Thread   0/ 10 with  1 cpus: 96
lassen16 Task   2/  4 Thread   0/ 10 with  1 gpus: 0x3
lassen16 Task   2/  4 Thread   9/ 10 with  1 cpus: 132
lassen16 Task   2/  4 Thread   9/ 10 with  1 gpus: 0x3
lassen16 Task   2/  4 Thread   7/ 10 with  1 cpus: 124
lassen16 Task   2/  4 Thread   7/ 10 with  1 gpus: 0x3
lassen16 Task   2/  4 Thread   1/ 10 with  1 cpus: 100
lassen16 Task   2/  4 Thread   1/ 10 with  1 gpus: 0x3
lassen16 Task   2/  4 Thread   5/ 10 with  1 cpus: 116
lassen16 Task   2/  4 Thread   5/ 10 with  1 gpus: 0x3
lassen16 Task   2/  4 Thread   8/ 10 with  1 cpus: 128
lassen16 Task   2/  4 Thread   8/ 10 with  1 gpus: 0x3
lassen16 Task   2/  4 Thread   6/ 10 with  1 cpus: 120
lassen16 Task   2/  4 Thread   6/ 10 with  1 gpus: 0x3
lassen16 Task   1/  4 Thread   2/ 10 with  1 cpus: 56
lassen16 Task   1/  4 Thread   2/ 10 with  1 gpus: 0x5
lassen16 Task   1/  4 Thread   1/ 10 with  1 cpus: 52
lassen16 Task   1/  4 Thread   1/ 10 with  1 gpus: 0x5
lassen16 Task   1/  4 Thread   7/ 10 with  1 cpus: 76
lassen16 Task   1/  4 Thread   7/ 10 with  1 gpus: 0x5
lassen16 Task   1/  4 Thread   3/ 10 with  1 cpus: 60
lassen16 Task   1/  4 Thread   3/ 10 with  1 gpus: 0x5
lassen16 Task   1/  4 Thread   4/ 10 with  1 cpus: 64
lassen16 Task   1/  4 Thread   4/ 10 with  1 gpus: 0x5
lassen16 Task   1/  4 Thread   8/ 10 with  1 cpus: 80
lassen16 Task   1/  4 Thread   8/ 10 with  1 gpus: 0x5
lassen16 Task   1/  4 Thread   0/ 10 with  1 cpus: 48
lassen16 Task   1/  4 Thread   0/ 10 with  1 gpus: 0x5
lassen16 Task   1/  4 Thread   6/ 10 with  1 cpus: 72
lassen16 Task   1/  4 Thread   6/ 10 with  1 gpus: 0x5
lassen16 Task   1/  4 Thread   9/ 10 with  1 cpus: 84
lassen16 Task   1/  4 Thread   9/ 10 with  1 gpus: 0x5
lassen16 Task   1/  4 Thread   5/ 10 with  1 cpus: 68
lassen16 Task   1/  4 Thread   5/ 10 with  1 gpus: 0x5
```

</details>

In the above examples, you may have noticed that the threads generated are evenly distributed across the GPUs on each system. This may be a bit easier to see if we spawn only as many workers as GPUs. On `Corona`, where there are 8 GPUs, see what happens when we create 8 tasks (and no additional threads):

<details>
<summary>

```
corona172$ srun -n8 ./mpi
```

</summary>

```
corona172  Task   1/  8 running on 6 CPUs: 6-11
           Task   1/  8 has 1 GPUs: 0x43
corona172  Task   2/  8 running on 6 CPUs: 12-17
           Task   2/  8 has 1 GPUs: 0x3
corona172  Task   3/  8 running on 6 CPUs: 18-23
           Task   3/  8 has 1 GPUs: 0x27
corona172  Task   4/  8 running on 6 CPUs: 24-29
           Task   4/  8 has 1 GPUs: 0xe3
corona172  Task   5/  8 running on 6 CPUs: 30-35
           Task   5/  8 has 1 GPUs: 0xc3
corona172  Task   6/  8 running on 6 CPUs: 36-41
           Task   6/  8 has 1 GPUs: 0x83
corona172  Task   7/  8 running on 6 CPUs: 42-47
           Task   7/  8 has 1 GPUs: 0xa3
corona172  Task   0/  8 running on 6 CPUs: 0-5
           Task   0/  8 has 1 GPUs: 0x63
```

</details>

Each of the 8 tasks is bound to a different GPU.

Each of the above examples used `mpibind` by default. If you want to turn `mpibind` off in an environment that uses it by default, you can use the flag `--mpibind=off`:

<details>
<summary>

```
pascal30$ srun --mpibind=off -n4 ./mpi
```

</summary>

```
pascal30   Task   0/  4 running on 72 CPUs: 0-71
           Task   0/  4 has 2 GPUs: 0x4 0x7
pascal30   Task   2/  4 running on 72 CPUs: 0-71
           Task   2/  4 has 2 GPUs: 0x4 0x7
pascal30   Task   3/  4 running on 72 CPUs: 0-71
           Task   3/  4 has 2 GPUs: 0x4 0x7
pascal30   Task   1/  4 running on 72 CPUs: 0-71
           Task   1/  4 has 2 GPUs: 0x4 0x7
```

</details>

Without `mpibind`, we see that each task has access to all CPUs and all GPUs.

## Key concept 2: Avoid multiple NUMA domains per worker

`mpibind` offloads workers first at the NUMA domain level. This means that we'll never see workers spilling across NUMA domains. In other words, workers will only be shared by compute resources that are local to one another.

For example, on `Pascal`, we have 36 cores split across 2 NUMA domains, with cores `0-17` on the first NUMA domain and `18-35` on the second. When we run a program with two MPI tasks, we see that each task gets assigned to a different NUMA domain, with 18 CPUs each.

<details>
<summary>

```
$ srun -n2 mpi
```

</summary>

```
pascal26   Task   0/  2 running on 18 CPUs: 0-17
pascal26   Task   1/  2 running on 18 CPUs: 18-35
```

</details>

If we double the number of tasks on a node, we halve the number of cores assigned to each task. With 4 MPI tasks on `Pascal`, each task gets 9 cores:

<details>
<summary>

```
$ srun -n4 mpi
```

</summary>

```
pascal26   Task   0/  4 running on 9 CPUs: 0-8
pascal26   Task   2/  4 running on 9 CPUs: 18-26
pascal26   Task   3/  4 running on 9 CPUs: 27-35
pascal26   Task   1/  4 running on 9 CPUs: 9-17
```

</details>

If we were to think about load-balancing from the perspective of sharing the 36s cores across all workers, we'd expect that running a program with 3 MPI tasks would give us a mapping where each task gets 12 cores.

Instead, we see that with 3 tasks on a `Pascal` node, two out of the three tasks each get 9 CPUs and one of the tasks gets 18 CPUs:

<details>
<summary>

```
$ srun -n3 mpi
```

</summary>

```
pascal26   Task   0/  3 running on 9 CPUs: 0-8
pascal26   Task   2/  3 running on 18 CPUs: 18-35
pascal26   Task   1/  3 running on 9 CPUs: 9-17
```

</details>

This is because each NUMA node is assigned only integer numbers of tasks, where the first NUMA domain on `Pascal` was assigned 2 of 3 tasks and the second NUMA domain was assigned a single task.

For emphasis, look at the distribution of workers if we add threads into the picture. If we spawn 2 threads for each of 3 tasks, we see that 4 of the resulting 6 threads are on the first NUMA domain (CPUs 0 to 17) and only 2 are on the second NUMA domain (CPUs 18 to 35).

<details>
<summary>

```
$ OMP_NUM_THREADS=2 srun -n3 mpi+omp
```

</summary>

```
pascal26 Task   0/  3 Thread   0/  2 with  1 cpus: 0
pascal26 Task   0/  3 Thread   1/  2 with  1 cpus: 5
pascal26 Task   1/  3 Thread   0/  2 with  1 cpus: 9
pascal26 Task   1/  3 Thread   1/  2 with  1 cpus: 14
pascal26 Task   2/  3 Thread   0/  2 with  1 cpus: 18
pascal26 Task   2/  3 Thread   1/  2 with  1 cpus: 27
```

</details>

## Key concept 3: Maximize cache per worker

As described in **Demo 1**, tasks are first distributed across NUMA domains. What happens after this? 

In distributing tasks across a single NUMA domain, workers are distributed to maximize the use of cache by each worker --- first at the L3 cache level and later at the L2 cache level. Let's see examples of this happening at the L3 cache level.

### 3.A: Maximizing `L3` on Corona 

On `Corona`, every 3 cores share an `L3` cache; with 34 cores per processor, there are 8 `L3` caches per processor and 16 `L3` caches per node.

Working on a single node of `Corona`, consider what happens when we work with 2 tasks (1 task per processor) and varying numbers of threads per task. For simplicity, let's consider the thread mappings we see across a single processor.

Starting with 4 threads on our first processor, we see that the numbers of the PUs assigned to each thread (the rightmost column) are all divisible by 3. Given the numbering scheme on `Corona` and the fact that every 3 cores share a `L3` cache, this shows that each thread has been assigned to its own `L3` cache:

<details>
<summary>

```
$ OMP_NUM_THREADS=4 srun -n2 ./mpi+omp
```

</summary>

```
corona171 Task   0/  2 Thread   0/  4 with  1 cpus: 0
corona171 Task   0/  2 Thread   1/  4 with  1 cpus: 6
corona171 Task   0/  2 Thread   2/  4 with  1 cpus: 12
corona171 Task   0/  2 Thread   3/  4 with  1 cpus: 18
...
```

</details>

Moving to 7 and then 8 threads per processor, the numbers of the PUs to which threads are assigned are always divisible by 3. This indicates that adding new threads has not caused any threads to share `L3` cache:

<details>
<summary>

```
$ OMP_NUM_THREADS=7 srun -n2 ./mpi+omp
```

</summary>

```
corona171 Task   0/  2 Thread   0/  7 with  1 cpus: 0
corona171 Task   0/  2 Thread   1/  7 with  1 cpus: 3
corona171 Task   0/  2 Thread   2/  7 with  1 cpus: 6
corona171 Task   0/  2 Thread   3/  7 with  1 cpus: 9
corona171 Task   0/  2 Thread   4/  7 with  1 cpus: 15
corona171 Task   0/  2 Thread   5/  7 with  1 cpus: 18
corona171 Task   0/  2 Thread   6/  7 with  1 cpus: 21
(...)
```

</details>

<details>
<summary>

```
$ OMP_NUM_THREADS=8 srun -n2 ./mpi+omp
```

</summary>

```
corona171 Task   0/  2 Thread   0/  8 with  1 cpus: 0
corona171 Task   0/  2 Thread   1/  8 with  1 cpus: 3
corona171 Task   0/  2 Thread   2/  8 with  1 cpus: 6
corona171 Task   0/  2 Thread   3/  8 with  1 cpus: 9
corona171 Task   0/  2 Thread   4/  8 with  1 cpus: 12
corona171 Task   0/  2 Thread   5/  8 with  1 cpus: 15
corona171 Task   0/  2 Thread   6/  8 with  1 cpus: 18
corona171 Task   0/  2 Thread   7/  8 with  1 cpus: 21
...
```

</details>

On the other hand, once we place 9 threads on the same processor, a renumbering occurs. Now we see that threads have been assigned to PUs 19 and 22:

<details>
<summary>

```
$ OMP_NUM_THREADS=9 srun -n2 ./mpi+omp
```

</summary>

```
corona171 Task   0/  2 Thread   0/  9 with  1 cpus: 0
corona171 Task   0/  2 Thread   1/  9 with  1 cpus: 2
corona171 Task   0/  2 Thread   2/  9 with  1 cpus: 5
corona171 Task   0/  2 Thread   3/  9 with  1 cpus: 8
corona171 Task   0/  2 Thread   4/  9 with  1 cpus: 11
corona171 Task   0/  2 Thread   5/  9 with  1 cpus: 13
corona171 Task   0/  2 Thread   6/  9 with  1 cpus: 16
corona171 Task   0/  2 Thread   7/  9 with  1 cpus: 19
corona171 Task   0/  2 Thread   8/  9 with  1 cpus: 22
...
```

</details>

Because the number of threads now exceeds the number of `L3` caches, threads begin to be assigned at the `L2` cache level. In other words, some threads are forced to share `L3` cache. (In particular, PUs 16 and 19 are on the same `L3` cache, so threads 6 and 7 are sharing `L3` cache.)

### 3.B: Maximizing `L3` on Lassen

On `Lassen`, every 2 cores share an `L3` cache; with 20 cores per processor, there are 10 `L3` caches per processor. 

The numbering scheme for `Lassen`'s PUs is a bit different than Coronas. `Lassen` has 4 PUs per core and these PUs are numbered sequentially. This means that every 4th PU is on a different core and, with 2 cores per `L3` cache, every 8th PU is on a different `L3` cache.

As on `Corona`, let's look at what we see when we run a program with 2 tasks, placing one on each node. Let's consider how threads map to a single processor, starting with 4 threads:

<details>
<summary>

```
$ OMP_NUM_THREADS=4 lrun -T2 ./mpi+omp
```

</summary>

```
lassen31 Task   0/  2 Thread   0/  4 with  1 cpus: 8
lassen31 Task   0/  2 Thread   1/  4 with  1 cpus: 32
lassen31 Task   0/  2 Thread   2/  4 with  1 cpus: 56
lassen31 Task   0/  2 Thread   3/  4 with  1 cpus: 72
```

</details>

Again in the rightmost column, we see the numberings of the PUs to which threads have been assigned; all PUs listed are divisible by 8, indicating that each thread has been bound to its own `L3` cache. This is true for up to 10 threads per processor. See the outputs below for bindings of 8, 9, and 10 threads on a single processor:

<details>
<summary>

```
$ OMP_NUM_THREADS=8 lrun -T2 ./mpi+omp
```

</summary>

```
lassen31 Task   0/  2 Thread   0/  8 with  1 cpus: 8
lassen31 Task   0/  2 Thread   1/  8 with  1 cpus: 24
lassen31 Task   0/  2 Thread   2/  8 with  1 cpus: 40
lassen31 Task   0/  2 Thread   3/  8 with  1 cpus: 48
lassen31 Task   0/  2 Thread   4/  8 with  1 cpus: 56
lassen31 Task   0/  2 Thread   5/  8 with  1 cpus: 64
lassen31 Task   0/  2 Thread   6/  8 with  1 cpus: 72
lassen31 Task   0/  2 Thread   7/  8 with  1 cpus: 80
(...)
```

</details>

<details>
<summary>

```
$ OMP_NUM_THREADS=9 lrun -T2 ./mpi+omp
```

</summary>

```
lassen31 Task   0/  2 Thread   0/  9 with  1 cpus: 8
lassen31 Task   0/  2 Thread   1/  9 with  1 cpus: 24
lassen31 Task   0/  2 Thread   2/  9 with  1 cpus: 32
lassen31 Task   0/  2 Thread   3/  9 with  1 cpus: 40
lassen31 Task   0/  2 Thread   4/  9 with  1 cpus: 48
lassen31 Task   0/  2 Thread   5/  9 with  1 cpus: 56
lassen31 Task   0/  2 Thread   6/  9 with  1 cpus: 64
lassen31 Task   0/  2 Thread   7/  9 with  1 cpus: 72
lassen31 Task   0/  2 Thread   8/  9 with  1 cpus: 80
(...)
```

</details>


<details>
<summary>

```
$ OMP_NUM_THREADS=10 lrun -T2 ./mpi+omp
```

</summary>

```
lassen31 Task   0/  2 Thread   0/ 10 with  1 cpus: 8
lassen31 Task   0/  2 Thread   1/ 10 with  1 cpus: 16
lassen31 Task   0/  2 Thread   2/ 10 with  1 cpus: 24
lassen31 Task   0/  2 Thread   3/ 10 with  1 cpus: 32
lassen31 Task   0/  2 Thread   4/ 10 with  1 cpus: 40
lassen31 Task   0/  2 Thread   5/ 10 with  1 cpus: 48
lassen31 Task   0/  2 Thread   6/ 10 with  1 cpus: 56
lassen31 Task   0/  2 Thread   7/ 10 with  1 cpus: 64
lassen31 Task   0/  2 Thread   8/ 10 with  1 cpus: 72
lassen31 Task   0/  2 Thread   9/ 10 with  1 cpus: 80
(...)
```

</details>

On the other hand, once we exceed 10 threads per processor, we see that thread 10 has been assigned to PU 84 (which isn't divisible by 8):

<details>
<summary>

```
$ OMP_NUM_THREADS=11 lrun -T2 ./mpi+omp
```

</summary>

```
lassen31 Task   0/  2 Thread   0/ 11 with  1 cpus: 8
lassen31 Task   0/  2 Thread   1/ 11 with  1 cpus: 16
lassen31 Task   0/  2 Thread   2/ 11 with  1 cpus: 24
lassen31 Task   0/  2 Thread   3/ 11 with  1 cpus: 32
lassen31 Task   0/  2 Thread   4/ 11 with  1 cpus: 40
lassen31 Task   0/  2 Thread   5/ 11 with  1 cpus: 48
lassen31 Task   0/  2 Thread   6/ 11 with  1 cpus: 56
lassen31 Task   0/  2 Thread   7/ 11 with  1 cpus: 64
lassen31 Task   0/  2 Thread   8/ 11 with  1 cpus: 72
lassen31 Task   0/  2 Thread   9/ 11 with  1 cpus: 80
lassen31 Task   0/  2 Thread  10/ 11 with  1 cpus: 84
```

</details>

Referring to the diagram of Lassen's topology under `Example Architectures`, you'll see that PU 80 and PU 84 are on Cores 88 and 92 of `Lassen`, which share an `L3` cache. Threads are now being assigned at the `L1` cache level. (Typically, `mpibind` would advance from assigning workers at the `L3` level to assigning them at the `L2` level, but the same groups of cores share both `L3` and `L2` in `Lassen`'s case.)


**Note** that the results in `Demo 3.A` and `Demo 3.B` have been modified for readibility; tasks were re-ordered numerically so changes to the mappings could be more easily seen.

**Comprehension check:** *Why wouldn't the demos in `Demo 3.A` or `Demo 3.B` have been very interesting on `Pascal`'s architecture, given what we already covered under **Key Concept 2**?*

### 3.C: Maximizing `L2` and `L1` on Corona 

On `Corona`, each core has its own `L2` and `L1` cache. Once `mpibind` starts assigning workers at the `L2` (and therefore also the `L1`) level, `mpibind` maximizes the `L2` and `L1` cache per worker. This means that no two workers are assigned to the same core until the number of workers exceeds the number of cores, even though there are two PUs per core that could handle separate threads.

For example, consider what happens if we run with 16 tasks at 3 threads each, via `OMP_NUM_THREADS=3 srun -n16 ./mpi+omp`. We'll see that each task is assigned to its own `L3` cache and that, on that `L3` cache, each thread is assigned to its own `L1` and `L2` cache. (Note that the PU numbering maxes out at 47, where the first PUs on all cores are numbered from 0 to 47 and the second PUs across all cores are numbered from 48 to 95.)

<details>
<summary>

```
$ OMP_NUM_THREADS=3 srun -n16 ./mpi+omp
```

</summary>

```
corona171 Task   4/ 16 Thread   1/  3 with  1 cpus: 13
corona171 Task   4/ 16 Thread   2/  3 with  1 cpus: 14
corona171 Task   4/ 16 Thread   0/  3 with  1 cpus: 12
corona171 Task  12/ 16 Thread   0/  3 with  1 cpus: 36
corona171 Task  12/ 16 Thread   1/  3 with  1 cpus: 37
corona171 Task  12/ 16 Thread   2/  3 with  1 cpus: 38
corona171 Task  14/ 16 Thread   0/  3 with  1 cpus: 42
corona171 Task  14/ 16 Thread   1/  3 with  1 cpus: 43
corona171 Task  14/ 16 Thread   2/  3 with  1 cpus: 44
corona171 Task   0/ 16 Thread   2/  3 with  1 cpus: 2
corona171 Task   0/ 16 Thread   1/  3 with  1 cpus: 1
corona171 Task   0/ 16 Thread   0/  3 with  1 cpus: 0
corona171 Task   2/ 16 Thread   0/  3 with  1 cpus: 6
corona171 Task   2/ 16 Thread   2/  3 with  1 cpus: 8
corona171 Task   2/ 16 Thread   1/  3 with  1 cpus: 7
corona171 Task   9/ 16 Thread   1/  3 with  1 cpus: 28
corona171 Task   9/ 16 Thread   0/  3 with  1 cpus: 27
corona171 Task   9/ 16 Thread   2/  3 with  1 cpus: 29
corona171 Task  10/ 16 Thread   0/  3 with  1 cpus: 30
corona171 Task  10/ 16 Thread   1/  3 with  1 cpus: 31
corona171 Task  10/ 16 Thread   2/  3 with  1 cpus: 32
corona171 Task   6/ 16 Thread   1/  3 with  1 cpus: 19
corona171 Task   6/ 16 Thread   0/  3 with  1 cpus: 18
corona171 Task   6/ 16 Thread   2/  3 with  1 cpus: 20
corona171 Task   8/ 16 Thread   0/  3 with  1 cpus: 24
corona171 Task   8/ 16 Thread   2/  3 with  1 cpus: 26
corona171 Task   8/ 16 Thread   1/  3 with  1 cpus: 25
corona171 Task  11/ 16 Thread   0/  3 with  1 cpus: 33
corona171 Task  11/ 16 Thread   2/  3 with  1 cpus: 35
corona171 Task  11/ 16 Thread   1/  3 with  1 cpus: 34
corona171 Task   3/ 16 Thread   1/  3 with  1 cpus: 10
corona171 Task   3/ 16 Thread   0/  3 with  1 cpus: 9
corona171 Task   3/ 16 Thread   2/  3 with  1 cpus: 11
corona171 Task   5/ 16 Thread   1/  3 with  1 cpus: 16
corona171 Task   5/ 16 Thread   2/  3 with  1 cpus: 17
corona171 Task   5/ 16 Thread   0/  3 with  1 cpus: 15
corona171 Task  13/ 16 Thread   0/  3 with  1 cpus: 39
corona171 Task  13/ 16 Thread   1/  3 with  1 cpus: 40
corona171 Task  13/ 16 Thread   2/  3 with  1 cpus: 41
corona171 Task  15/ 16 Thread   0/  3 with  1 cpus: 45
corona171 Task  15/ 16 Thread   1/  3 with  1 cpus: 46
corona171 Task  15/ 16 Thread   2/  3 with  1 cpus: 47
corona171 Task   7/ 16 Thread   1/  3 with  1 cpus: 22
corona171 Task   7/ 16 Thread   0/  3 with  1 cpus: 21
corona171 Task   7/ 16 Thread   2/  3 with  1 cpus: 23
corona171 Task   1/ 16 Thread   1/  3 with  1 cpus: 4
corona171 Task   1/ 16 Thread   2/  3 with  1 cpus: 5
corona171 Task   1/ 16 Thread   0/  3 with  1 cpus: 3
```

</details>

It's only once we move from 3 threads per `L3` cache to 4 threads per `L3` cache that we start to see threads sharing a single `L1` and `L2` cache. For example, in the output below, we see that Task 0's threads are assigned to PUs 0, 48, 49, and 50. We know from our diagram of `Corona`'s topology that PUs 0 and 48 are both on Core 0 and share a `L1` and `L2` cache.

Furthermore, note that in the numbering in the rightmost column below, we see threads bound to PUs ranging from 0 to 95. 

<details>
<summary>

```
$ OMP_NUM_THREADS=4 srun -n16 ./mpi+omp
```

</summary>

```
corona171 Task   3/ 16 Thread   1/  4 with  1 cpus: 57
corona171 Task   3/ 16 Thread   0/  4 with  1 cpus: 9
corona171 Task   3/ 16 Thread   2/  4 with  1 cpus: 58
corona171 Task   3/ 16 Thread   3/  4 with  1 cpus: 59
corona171 Task   6/ 16 Thread   0/  4 with  1 cpus: 18
corona171 Task   6/ 16 Thread   3/  4 with  1 cpus: 68
corona171 Task   6/ 16 Thread   1/  4 with  1 cpus: 66
corona171 Task   6/ 16 Thread   2/  4 with  1 cpus: 67
corona171 Task   8/ 16 Thread   0/  4 with  1 cpus: 24
corona171 Task   8/ 16 Thread   3/  4 with  1 cpus: 74
corona171 Task   8/ 16 Thread   2/  4 with  1 cpus: 73
corona171 Task   8/ 16 Thread   1/  4 with  1 cpus: 72
corona171 Task   0/ 16 Thread   2/  4 with  1 cpus: 49
corona171 Task   0/ 16 Thread   3/  4 with  1 cpus: 50
corona171 Task   0/ 16 Thread   1/  4 with  1 cpus: 48
corona171 Task   0/ 16 Thread   0/  4 with  1 cpus: 0
corona171 Task  10/ 16 Thread   0/  4 with  1 cpus: 30
corona171 Task  10/ 16 Thread   3/  4 with  1 cpus: 80
corona171 Task  10/ 16 Thread   2/  4 with  1 cpus: 79
corona171 Task  10/ 16 Thread   1/  4 with  1 cpus: 78
corona171 Task  15/ 16 Thread   0/  4 with  1 cpus: 45
corona171 Task  15/ 16 Thread   1/  4 with  1 cpus: 93
corona171 Task  15/ 16 Thread   3/  4 with  1 cpus: 95
corona171 Task  15/ 16 Thread   2/  4 with  1 cpus: 94
corona171 Task  13/ 16 Thread   0/  4 with  1 cpus: 39
corona171 Task  13/ 16 Thread   3/  4 with  1 cpus: 89
corona171 Task  13/ 16 Thread   2/  4 with  1 cpus: 88
corona171 Task  13/ 16 Thread   1/  4 with  1 cpus: 87
corona171 Task  11/ 16 Thread   0/  4 with  1 cpus: 33
corona171 Task  11/ 16 Thread   2/  4 with  1 cpus: 82
corona171 Task  11/ 16 Thread   3/  4 with  1 cpus: 83
corona171 Task  11/ 16 Thread   1/  4 with  1 cpus: 81
corona171 Task   1/ 16 Thread   1/  4 with  1 cpus: 51
corona171 Task   1/ 16 Thread   2/  4 with  1 cpus: 52
corona171 Task   1/ 16 Thread   3/  4 with  1 cpus: 53
corona171 Task   1/ 16 Thread   0/  4 with  1 cpus: 3
corona171 Task   9/ 16 Thread   0/  4 with  1 cpus: 27
corona171 Task   9/ 16 Thread   3/  4 with  1 cpus: 77
corona171 Task   9/ 16 Thread   2/  4 with  1 cpus: 76
corona171 Task   9/ 16 Thread   1/  4 with  1 cpus: 75
corona171 Task   2/ 16 Thread   0/  4 with  1 cpus: 6
corona171 Task   2/ 16 Thread   1/  4 with  1 cpus: 54
corona171 Task   2/ 16 Thread   3/  4 with  1 cpus: 56
corona171 Task   2/ 16 Thread   2/  4 with  1 cpus: 55
corona171 Task   4/ 16 Thread   0/  4 with  1 cpus: 12
corona171 Task   4/ 16 Thread   2/  4 with  1 cpus: 61
corona171 Task   4/ 16 Thread   3/  4 with  1 cpus: 62
corona171 Task   4/ 16 Thread   1/  4 with  1 cpus: 60
corona171 Task   5/ 16 Thread   0/  4 with  1 cpus: 15
corona171 Task   5/ 16 Thread   2/  4 with  1 cpus: 64
corona171 Task   5/ 16 Thread   1/  4 with  1 cpus: 63
corona171 Task   5/ 16 Thread   3/  4 with  1 cpus: 65
corona171 Task  12/ 16 Thread   3/  4 with  1 cpus: 86
corona171 Task  12/ 16 Thread   1/  4 with  1 cpus: 84
corona171 Task  12/ 16 Thread   2/  4 with  1 cpus: 85
corona171 Task  12/ 16 Thread   0/  4 with  1 cpus: 36
corona171 Task  14/ 16 Thread   0/  4 with  1 cpus: 42
corona171 Task  14/ 16 Thread   2/  4 with  1 cpus: 91
corona171 Task  14/ 16 Thread   1/  4 with  1 cpus: 90
corona171 Task  14/ 16 Thread   3/  4 with  1 cpus: 92
corona171 Task   7/ 16 Thread   0/  4 with  1 cpus: 21
corona171 Task   7/ 16 Thread   1/  4 with  1 cpus: 69
corona171 Task   7/ 16 Thread   3/  4 with  1 cpus: 71
corona171 Task   7/ 16 Thread   2/  4 with  1 cpus: 70
```

</details>

<!--
***Note:** Another command we could use to print affinity:*
```
srun -n 10 python3 -c "import os; print(os.sched_getaffinity(0))"
```
-->

**Comprehension check:** *On `Corona`, `L1` and `L2` cache use are optimized by the same mappings. Is the same true on Lassen?*

## Key concept 4: Leverage GPU locality 

An optimized mapping for a given application will leverage either CPU or GPU locality, depending on whether CPUs or GPUs are the most important resource for that application. On heterogeneous systems with CPUs and GPUs, mpibind needs to know which is the most important resource for an application to create an optimized mapping.

For GPU-intensive applications, mpibind provides the `gpu` option to
create mappings that make GPUs the focal point (rather than CPUs) and
leverage GPU locality of the node topology. For more information see
*Mapping MPI+X applications to multi-GPU architectures: A
performance-portable approach* [GTC'18].

Let's look at why **locality matters** to performance optimization, using bandwidth tests on `Corona` as a case study. 

Recall that Corona has 8 GPUs: 4 local to NUMA domain 0 (rsmi[0-3]) and 4 local to NUMA domain 1 (rsmi[4-7]). Below we measure the bandwidth of transferring data between 

1. NUMA domain 0 and a local GPU; 
2. NUMA domain 0 and a remote GPU; 
3. two GPUs local to the same NUMA domain; and 
4. two GPUs, one local to NUMA domain 0 and the other local to NUMA domain 1.

<!--
GPU[0]		: PCI Bus: 0000:63:00.0
GPU[1]		: PCI Bus: 0000:43:00.0
GPU[4]		: PCI Bus: 0000:E3:00.0
-->

### 4.A Bandwidth between NUMA domain 0 and GPU 0

Here we look at bandwidth measurements for data passed between a CPU on NUMA domain 0 and a local GPU. In the leftmost column, the size of the data transferred is shown. For simplicity we can focus on the resulting peak bandwidth measurements in the rightmost column. 

For the data transfers shown, peak bandwidth increases with increasing data size but starts to plateau a little over 51 GB/s at a data size of about 8 MB. Peak bandwidth is nearly 52 GB/s for a data size of 512 MB.

```
........................................
          RocmBandwidthTest Version: 2.6.0

          Launch Command is: rocm-bandwidth-test -b 0,2


================    Bidirectional Benchmark Result    ================
================ Src Device Id: 0 Src Device Type: Cpu ================
================ Dst Device Id: 2 Dst Device Type: Gpu ================

Data Size      Avg Time(us)   Avg BW(GB/s)   Min Time(us)   Peak BW(GB/s)  
1 KB           4.920          0.416          4.640          0.441          
2 KB           4.760          0.861          4.640          0.883          
4 KB           4.920          1.665          4.800          1.707          
8 KB           5.040          3.251          4.800          3.413          
16 KB          5.400          6.068          5.280          6.206          
32 KB          6.360          10.304         6.240          10.503         
64 KB          7.600          17.246         7.040          18.618         
128 KB         9.760          26.859         9.600          27.307         
256 KB         14.680         35.715         14.399         36.411         
512 KB         24.520         42.764         24.480         42.834         
1 MB           45.000         46.603         44.640         46.979         
2 MB           85.840         48.862         85.439         49.091         
4 MB           167.360        50.123         166.720        50.316         
8 MB           328.600        51.057         328.160        51.125         
16 MB          651.919        51.470         651.679        51.489         
32 MB          1297.958       51.703         1297.279       51.730         
64 MB          2587.876       51.864         2587.196       51.878         
128 MB         5171.233       51.909         5170.552       51.916         
256 MB         10337.869      51.932         10337.269      51.935         
512 MB         20669.251      51.949         20668.778      51.950 
```

### 4.B Bandwidth between NUMA domain 0 and GPU 4

Let's compare the bandwidth measurements we see for data transferred between a CPU on NUMA domain 0 and a remote GPU. Here, the peak bandwidth is just over 34 GB/s and occurs at a data size of 128 MB. 

In both the bandwidth tests where data was sent to a local GPU and in those below where data is sent to a remote GPU, average bandwidth tracks with and is fairly similar to peak bandwidth. 

By both metrics, bandwidth is greater when data is transferred between a CPU and a local GPU than between a CPU and remote GPU.

```
........................................
          RocmBandwidthTest Version: 2.6.0

          Launch Command is: rocm-bandwidth-test -b 0,6


================    Bidirectional Benchmark Result    ================
================ Src Device Id: 0 Src Device Type: Cpu ================
================ Dst Device Id: 6 Dst Device Type: Gpu ================

Data Size      Avg Time(us)   Avg BW(GB/s)   Min Time(us)   Peak BW(GB/s)  
1 KB           5.160          0.397          4.800          0.427          
2 KB           5.160          0.794          4.960          0.826          
4 KB           5.200          1.575          5.120          1.600          
8 KB           5.720          2.864          5.600          2.926          
16 KB          6.240          5.251          5.760          5.689          
32 KB          7.200          9.102          6.880          9.526          
64 KB          8.880          14.760         7.840          16.718         
128 KB         13.320         19.680         13.120         19.980         
256 KB         20.440         25.650         20.000         26.214         
512 KB         36.080         29.063         35.840         29.257         
1 MB           67.160         31.226         66.880         31.357         
2 MB           128.560        32.625         128.160        32.727         
4 MB           251.720        33.325         251.040        33.415         
8 MB           498.519        33.654         497.280        33.738         
16 MB          990.319        33.882         990.079        33.891         
32 MB          1976.838       33.948         1975.838       33.965         
64 MB          3947.556       34.000         3943.196       34.038         
128 MB         7889.954       34.022         7886.075       34.039         
256 MB         15885.667      33.796         15871.988      33.825         
512 MB         31787.254      33.779         31765.255      33.802         
```
### 4.C Bandwidth between GPUs 0 and 1 

In this example, bandwidth measurements are taken when transferring data between two local GPUs -- GPUs associated with the same NUMA domain. Here we see that the max bandwidth achieved is just under 56 GB/s.

```
........................................
          RocmBandwidthTest Version: 2.6.0

          Launch Command is: rocm-bandwidth-test -b 2,3


================    Bidirectional Benchmark Result    ================
================ Src Device Id: 2 Src Device Type: Gpu ================
================ Dst Device Id: 3 Dst Device Type: Gpu ================

Data Size      Avg Time(us)   Avg BW(GB/s)   Min Time(us)   Peak BW(GB/s)  
1 KB           5.070          0.404          4.710          0.435          
2 KB           5.452          0.751          4.480          0.914          
4 KB           5.831          1.405          5.670          1.445          
8 KB           5.991          2.735          5.831          2.810          
16 KB          5.733          5.715          5.120          6.400          
32 KB          7.471          8.772          7.431          8.819          
64 KB          9.471          13.839         9.031          14.514         
128 KB         13.311         19.694         13.191         19.873         
256 KB         20.831         25.169         20.391         25.712         
512 KB         37.213         28.178         33.510         31.291         
1 MB           65.293         32.119         65.191         32.169         
2 MB           129.156        32.475         128.800        32.564         
4 MB           163.493        51.309         154.471        54.305         
8 MB           303.632        55.255         303.432        55.292         
16 MB          603.956        55.558         603.286        55.619         
32 MB          1203.336       55.769         1202.560       55.805         
64 MB          2401.177       55.897         2400.638       55.909         
128 MB         4797.917       55.948         4797.595       55.952         
256 MB         9592.825       55.966         9592.469       55.968         
512 MB         19186.689      55.963         19186.367      55.964         
```

### 4.D Bandwidth between GPUs 0 and 4

Finally, below we see bandwidth measurements for data transferred between remote GPUs -- GPUs associated with different NUMA domains. Here we see the peak bandwidth is about 42.5 GB/s, only a bit over 75% of the max bandwidth achievable between local GPUs.

```
........................................
          RocmBandwidthTest Version: 2.6.0

          Launch Command is: rocm-bandwidth-test -b 2,6


================    Bidirectional Benchmark Result    ================
================ Src Device Id: 2 Src Device Type: Gpu ================
================ Dst Device Id: 6 Dst Device Type: Gpu ================

Data Size      Avg Time(us)   Avg BW(GB/s)   Min Time(us)   Peak BW(GB/s)  
1 KB           4.420          0.463          4.060          0.504          
2 KB           4.739          0.864          4.219          0.971          
4 KB           5.100          1.606          4.540          1.804          
8 KB           5.060          3.238          4.700          3.486          
16 KB          4.764          6.878          4.379          7.483          
32 KB          5.700          11.498         5.500          11.916         
64 KB          7.420          17.665         7.100          18.461         
128 KB         9.920          26.426         9.700          27.025         
256 KB         17.610         29.772         17.280         30.341         
512 KB         33.290         31.498         33.120         31.660         
1 MB           65.170         32.180         65.060         32.234         
2 MB           101.225        41.436         101.060        41.503         
4 MB           200.220        41.897         200.100        41.922         
8 MB           397.369        42.221         397.177        42.241         
16 MB          791.931        42.370         791.778        42.379         
32 MB          1580.645       42.457         1580.576       42.458         
64 MB          3158.438       42.495         3157.957       42.501         
128 MB         6548.843       40.990         6395.061       41.975         
256 MB         13294.128      40.384         13223.614      40.599         
512 MB         26599.198      40.367         26592.208      40.378         
```


### Takeaways

The best performance is achieved when using local
GPUs. Furthermore, the performance loss as a result of remote accesses
is significant!

With mpibind applications leverage GPU locality as exemplified
below.

On `Corona`, no tasks are bound to GPUs spanning the two sockets. For 4 tasks and 8 GPUs, each task is bound to two GPUs on the same socket.

<details>
<summary>

```
# Corona
# Socket 0: CPUs  0-24, GPUs 0x63,0x43,0x3,0x27
# Socket 1: CPUs 24-47, GPUs 0xe3,0xc3,0x83,0xa3

$ MPIBIND=gpu srun -n4 ./mpi
```

</summary>

```
corona171  Task   0/  4 running on 12 CPUs: 0-11
           Task   0/  4 has 2 GPUs: 0x63 0x43 
corona171  Task   1/  4 running on 12 CPUs: 12-23
           Task   1/  4 has 2 GPUs: 0x3 0x27 
corona171  Task   2/  4 running on 12 CPUs: 24-35
           Task   2/  4 has 2 GPUs: 0xe3 0xc3 
corona171  Task   3/  4 running on 12 CPUs: 36-47
           Task   3/  4 has 2 GPUs: 0x83 0xa3 
```

</details>

How many GPUs would you expect each task to receive if we ran a program with only 3 tasks per `Corona` node? 

If we wanted to load balance the number of GPUs per task, 2 tasks would be bound to 3 GPUs each and the final task would be bound to only 2 GPUs each. However, this would require that one task would use two local and one remote GPUs, instead of all local GPUs.

Instead, with `MPIBIND=gpu`, each task again uses only local GPUs, even though that means the distribution of GPUs across tasks is less balanced:

<details>
<summary>

```
$ MPIBIND=gpu srun -n3 ./mpi
```

</summary>

```
corona171  Task   0/  3 running on 12 CPUs: 0-11
           Task   0/  3 has 2 GPUs: 0x63 0x43
corona171  Task   2/  3 running on 24 CPUs: 24-47
           Task   2/  3 has 4 GPUs: 0xe3 0xc3 0x83 0xa3
corona171  Task   1/  3 running on 12 CPUs: 12-23
           Task   1/  3 has 2 GPUs: 0x3 0x27
```

</details>

Finally, we see that when we optimize for GPU locality, the mapping of tasks to a `Pascal` node has all tasks running on a single socket:  

<details>
<summary>

```
# Pascal
# Socket 0: CPUs  0-17 GPUs 0x4,0x7
# Socket 1: CPUs 18-35 

$ MPIBIND=gpu srun -n4 ./mpi
```

</summary>

```
pascal10   Task   0/  4 running on 5 CPUs: 0-4
           Task   0/  4 has 1 GPUs: 0x4 
pascal10   Task   1/  4 running on 5 CPUs: 5-9
           Task   1/  4 has 1 GPUs: 0x4 
pascal10   Task   2/  4 running on 4 CPUs: 10-13
           Task   2/  4 has 1 GPUs: 0x7 
pascal10   Task   3/  4 running on 4 CPUs: 14-17
           Task   3/  4 has 1 GPUs: 0x7 
```

</details>

On `Pascal`, all GPUs are on Socket 0 and GPU locality is prioritized even though all the CPUs on Socket 1 are left idle.


## Key concept 5: Leverage CPU locality

In the previous section, we saw how GPU locality matters and how
mpibind leverages it. In this section, we focus on mappings optimized
for CPU-based applications. By default, mpibind
optimizes the mapping to maximize CPU cache and memory per
worker. For more information see *Achieving transparency mapping
parallel applications: A memory hierarchy affair* [MEMSYS'18].

To see the difference between CPU and GPU-optimized mappings, let's start by setting the `gpu` option on Pascal and measuring memory bandwidth on the CPU.

<details>
<summary>

```
$ MPIBIND=v.gpu srun -N1 -n2 bin/stream-mpi-128m.pascal
```

</summary>

```
mpibind.prolog pascal1: rank  0  gpu   0  cpu 0,1,2,3,4,5,6,7,8
mpibind.prolog pascal1: rank  1  gpu   1  cpu 9,10,11,12,13,14,15,16,17
-------------------------------------------------------------
STREAM version $Revision: 1.8 $
-------------------------------------------------------------
This system uses 8 bytes per array element.
-------------------------------------------------------------
Total Aggregate Array size = 128000000 (elements)
Total Aggregate Memory per array = 976.6 MiB (= 1.0 GiB).
Total Aggregate memory required = 2929.7 MiB (= 2.9 GiB).
Data is distributed across 2 MPI ranks
   Array size per MPI rank = 64000000 (elements)
   Memory per array per MPI rank = 488.3 MiB (= 0.5 GiB).
   Total memory per MPI rank = 1464.8 MiB (= 1.4 GiB).
-------------------------------------------------------------
Each kernel will be executed 10 times.
 The *best* time for each kernel (excluding the first iteration)
 will be used to compute the reported bandwidth.
The SCALAR value used for this run is 0.420000
-------------------------------------------------------------
Number of Threads requested for each MPI rank = 9
Number of Threads counted for rank 0 = 9
-------------------------------------------------------------
Your timer granularity/precision appears to be 1 microseconds.
Each test below will take on the order of 32154 microseconds.
   (= 32154 timer ticks)
Increase the size of the arrays if this shows that
you are not getting at least 20 timer ticks per test.
-------------------------------------------------------------
WARNING -- The above is only a rough guideline.
For best results, please be sure you know the
precision of your system timer.
-------------------------------------------------------------
Function    Best Rate MB/s  Avg time     Min time     Max time
Copy:          57167.1     0.038003     0.035825     0.044590
Scale:         43198.5     0.048267     0.047409     0.053323
Add:           48967.4     0.063645     0.062736     0.067301
Triad:         49134.6     0.064612     0.062522     0.068174
-------------------------------------------------------------
Solution Validates: avg error less than 1.000000e-13 on all three arrays
-------------------------------------------------------------
```

</details>

Notice that the two tasks run on Pascal's socket 0, because that's
where the GPUs are. For simplicity, let's look at the resulting memory bandwidth of the `Triad` function, given towards the end of the output above. The best memory bandwidth is a bit over 49,000 MB/s (49 GB/s).

Now, let's focus on the CPU optimized mapping (the
default in mpibind). Notice that the only difference between the command run above and that run below is that we change `MPIBIND=v.gpu` to `MPIBIND=v`.

<details>
<summary>

```
$ MPIBIND=v srun -N1 -n2 bin/stream-mpi-128m.pascal
```

</summary>

```
mpibind.prolog pascal1: rank  0  gpu 0  cpu 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17
mpibind.prolog pascal1: rank  1  gpu 1  cpu 18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35
-------------------------------------------------------------
STREAM version $Revision: 1.8 $
-------------------------------------------------------------
This system uses 8 bytes per array element.
-------------------------------------------------------------
Total Aggregate Array size = 128000000 (elements)
Total Aggregate Memory per array = 976.6 MiB (= 1.0 GiB).
Total Aggregate memory required = 2929.7 MiB (= 2.9 GiB).
Data is distributed across 2 MPI ranks
   Array size per MPI rank = 64000000 (elements)
   Memory per array per MPI rank = 488.3 MiB (= 0.5 GiB).
   Total memory per MPI rank = 1464.8 MiB (= 1.4 GiB).
-------------------------------------------------------------
Each kernel will be executed 10 times.
 The *best* time for each kernel (excluding the first iteration)
 will be used to compute the reported bandwidth.
The SCALAR value used for this run is 0.420000
-------------------------------------------------------------
Number of Threads requested for each MPI rank = 18
Number of Threads counted for rank 0 = 18
-------------------------------------------------------------
Your timer granularity/precision appears to be 1 microseconds.
Each test below will take on the order of 17776 microseconds.
   (= 17776 timer ticks)
Increase the size of the arrays if this shows that
you are not getting at least 20 timer ticks per test.
-------------------------------------------------------------
WARNING -- The above is only a rough guideline.
For best results, please be sure you know the
precision of your system timer.
-------------------------------------------------------------
Function    Best Rate MB/s  Avg time     Min time     Max time
Copy:         111136.1     0.018477     0.018428     0.018555
Scale:         86922.4     0.023684     0.023561     0.024114
Add:           97203.5     0.032239     0.031604     0.037152
Triad:         97767.0     0.031644     0.031422     0.032615
-------------------------------------------------------------
Solution Validates: avg error less than 1.000000e-13 on all three arrays
-------------------------------------------------------------
```

</details>

With a CPU-optimized mapping, the max memory bandwidth for the `Triad` function is close to 98 GB/s, approaching twice the memory bandwidth of the GPU-optimized mapping!

This significant increase in memory bandwidth occurs because the code 
leverages the bandwidth available to both sockets. As mentioned before,
mpibind assigns as much cache and memory as possible to
each worker.


<!-- Todo: I could use kokkos bytes_and_flops to show the
bandwidth/flops with and withou mpibind. There's a signifcant
performance differential.

Now, if the user launches a GPU-based job with two tasks, mpibind
optimizes the placement of the tasks to leverage GPU locality. In
other words, the tasks are placed on the first socket, remember that
on Pascal the two GPUs are attached to this socket. We do
this by adding the `gpu` option to mpibind:
-->



## Key concept 6: Leverage SMT support

On architectures with Simultaneous Multi-Threading (SMT), one can
leverage multiple hardware threads per core. On `Lassen`, an SMT-4
architecture, there are four hardware threads per core. A user may
want to use one or more of these threads, but note that the threads
share most of the core's compute capability such as the floating-point
unit. This means that using two threads may not double performance and so forth.

The benefits of SMT are application-dependent. An argument for using
more than one hardware thread is to mask memory latency and improve
overall performance. An argument for not using all of the threads
per core is to allow system services (e.g., OS services) to run
concurrently alongside the application. For more information see
*Enabling application scalability and reproducibility with SMT*
[IPDPS'16]. 

With mpibind a user can specify the desired SMT level using the
`smt<n>` option. By default mpibind uses one hardware thread per core for
application work. Continuing with our memory bandwidth benchmark,
let's see what happens when using SMT-1, SMT-2, and SMT-4 on `Lassen`.

<details>
<summary>

```
$ MPIBIND=v lrun -N1 -n2 bin/stream-mpi-128m.lassen
```

</summary>

```
mpibind10: lassen26  rank  0  gpu 0 1  cpu 8,12,16,20,24,28,32,36,40,44,48,52,56,60,64,68,72,76,80,84
mpibind10: lassen26  rank  1  gpu 2 3  cpu 96,100,104,108,112,116,120,124,128,132,136,140,144,148,152,156,160,164,168,172
-------------------------------------------------------------
STREAM version $Revision: 1.8 $
-------------------------------------------------------------
This system uses 8 bytes per array element.
-------------------------------------------------------------
Total Aggregate Array size = 128000000 (elements)
Total Aggregate Memory per array = 976.6 MiB (= 1.0 GiB).
Total Aggregate memory required = 2929.7 MiB (= 2.9 GiB).
Data is distributed across 2 MPI ranks
   Array size per MPI rank = 64000000 (elements)
   Memory per array per MPI rank = 488.3 MiB (= 0.5 GiB).
   Total memory per MPI rank = 1464.8 MiB (= 1.4 GiB).
-------------------------------------------------------------
Each kernel will be executed 10 times.
 The *best* time for each kernel (excluding the first iteration)
 will be used to compute the reported bandwidth.
The SCALAR value used for this run is 0.420000
-------------------------------------------------------------
Number of Threads requested for each MPI rank = 20
Number of Threads counted for rank 0 = 20
-------------------------------------------------------------
Your timer granularity/precision appears to be 1 microseconds.
Each test below will take on the order of 7429 microseconds.
   (= 7429 timer ticks)
Increase the size of the arrays if this shows that
you are not getting at least 20 timer ticks per test.
-------------------------------------------------------------
WARNING -- The above is only a rough guideline.
For best results, please be sure you know the
precision of your system timer.
-------------------------------------------------------------
Function    Best Rate MB/s  Avg time     Min time     Max time
Copy:         235620.4     0.008735     0.008692     0.008815
Scale:        250994.5     0.008201     0.008160     0.008245
Add:          246437.0     0.012520     0.012466     0.012588
Triad:        248859.4     0.012400     0.012344     0.012453
-------------------------------------------------------------
Solution Validates: avg error less than 1.000000e-13 on all three arrays
-------------------------------------------------------------
```

</details>

<details>
<summary>

```
$ MPIBIND=v.smt2 lrun -N1 -n2 bin/stream-mpi-128m.lassen
```

</summary>

```
mpibind10: lassen26  rank  0  gpu 0 1  cpu 8,9,12,13,16,17,20,21,24,25,28,29,32,33,36,37,40,41,44,45,48,49,52,53,56,57,60,61,64,65,68,69,72,73,76,77,80,81,84,85
mpibind10: lassen26  rank  1  gpu 2 3  cpu 96,97,100,101,104,105,108,109,112,113,116,117,120,121,124,125,128,129,132,133,136,137,140,141,144,145,148,149,152,153,156,157,160,161,164,165,168,169,172,173
-------------------------------------------------------------
STREAM version $Revision: 1.8 $
-------------------------------------------------------------
This system uses 8 bytes per array element.
-------------------------------------------------------------
Total Aggregate Array size = 128000000 (elements)
Total Aggregate Memory per array = 976.6 MiB (= 1.0 GiB).
Total Aggregate memory required = 2929.7 MiB (= 2.9 GiB).
Data is distributed across 2 MPI ranks
   Array size per MPI rank = 64000000 (elements)
   Memory per array per MPI rank = 488.3 MiB (= 0.5 GiB).
   Total memory per MPI rank = 1464.8 MiB (= 1.4 GiB).
-------------------------------------------------------------
Each kernel will be executed 10 times.
 The *best* time for each kernel (excluding the first iteration)
 will be used to compute the reported bandwidth.
The SCALAR value used for this run is 0.420000
-------------------------------------------------------------
Number of Threads requested for each MPI rank = 40
Number of Threads counted for rank 0 = 40
-------------------------------------------------------------
Your timer granularity/precision appears to be 1 microseconds.
Each test below will take on the order of 7616 microseconds.
   (= 7616 timer ticks)
Increase the size of the arrays if this shows that
you are not getting at least 20 timer ticks per test.
-------------------------------------------------------------
WARNING -- The above is only a rough guideline.
For best results, please be sure you know the
precision of your system timer.
-------------------------------------------------------------
Function    Best Rate MB/s  Avg time     Min time     Max time
Copy:         225546.4     0.009131     0.009080     0.009193
Scale:        221337.1     0.009295     0.009253     0.009334
Add:          234484.0     0.013198     0.013101     0.013275
Triad:        225920.4     0.013679     0.013598     0.013771
-------------------------------------------------------------
Solution Validates: avg error less than 1.000000e-13 on all three arrays
-------------------------------------------------------------
```

</details>

<details>
<summary>

```
$ MPIBIND=v.smt4 lrun -N1 -n2 bin/stream-mpi-128m.lassen
```

</summary>

```
mpibind10: lassen26  rank  0  gpu 0 1  cpu 8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87
mpibind10: lassen26  rank  1  gpu 2 3  cpu 96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175
-------------------------------------------------------------
STREAM version $Revision: 1.8 $
-------------------------------------------------------------
This system uses 8 bytes per array element.
-------------------------------------------------------------
Total Aggregate Array size = 128000000 (elements)
Total Aggregate Memory per array = 976.6 MiB (= 1.0 GiB).
Total Aggregate memory required = 2929.7 MiB (= 2.9 GiB).
Data is distributed across 2 MPI ranks
   Array size per MPI rank = 64000000 (elements)
   Memory per array per MPI rank = 488.3 MiB (= 0.5 GiB).
   Total memory per MPI rank = 1464.8 MiB (= 1.4 GiB).
-------------------------------------------------------------
Each kernel will be executed 10 times.
 The *best* time for each kernel (excluding the first iteration)
 will be used to compute the reported bandwidth.
The SCALAR value used for this run is 0.420000
-------------------------------------------------------------
Number of Threads requested for each MPI rank = 80
Number of Threads counted for rank 0 = 80
-------------------------------------------------------------
Your timer granularity/precision appears to be 1 microseconds.
Each test below will take on the order of 7637 microseconds.
   (= 7637 timer ticks)
Increase the size of the arrays if this shows that
you are not getting at least 20 timer ticks per test.
-------------------------------------------------------------
WARNING -- The above is only a rough guideline.
For best results, please be sure you know the
precision of your system timer.
-------------------------------------------------------------
Function    Best Rate MB/s  Avg time     Min time     Max time
Copy:         213808.2     0.009608     0.009579     0.009653
Scale:        193617.8     0.010621     0.010578     0.010687
Add:          215991.0     0.014311     0.014223     0.014382
Triad:        187266.5     0.016499     0.016404     0.016595
-------------------------------------------------------------
Solution Validates: avg error less than 1.000000e-13 on all three arrays
-------------------------------------------------------------
```

</details>

Focusing on `Triad` results, we observe memory  bandwidths of 248 GB/s with SMT-1, 225 GB/s with SMT-2, and 187 GB/s with SMT-4. Since 20 threads per NUMA domain already maximizes DRAM bandwidth, adding more threads puts excess
pressure on the cores and memory, resulting in performance loss.


<!-- We probably have enough. Commenting out OpenMP compatibility
## OpenMP compatibility

mpibind supports OpenMPI binding throught setting the `OMP_PROC_BIND`
environment variable. This variable is used to specify the mapping
policy for OpenMP threads and can take the values `master`, `close`,
and `spread`. By default mpibind uses a \emph{spread} policy, but
users can overwrite that behavior as shown below.

Can use mpibind with user defined OMP_PROC_BIND={master, close,
spread}. By default mpibind uses 'spread'
-->



<!-- Commenting out for now 
## Greedy: I want everything!


* Applies if the number of tasks is less than the number of NUMA
 domains 

* Working w/Python or custom workflow managers
-->


## Interfacing with mpibind 

<!--
Resource manager plugins and the C interface
--> 

The way we use mpibind in this module is through a resource manager
plugin that makes mpibind *invisible* to users: (1) users do not
need to make any changes to their applications or run commands and (2)
users can pass options to mpibind through an environment variable. To
see what plugins are available please see mpibind's repository on
GitHub. 

Advanced users may want to use mpibind directly in their C/C++ or
Python programs. In this case, mpibind provides C and Python
interfaces through a shared library. Examples of the C and Python
interfaces can be found at the GitHub repository as well as detailed
instructions on building and installing the shared library.


## Hands-on exercises

<!--
Todo: Need to expand.

1. Determine the resources associated with each task and thread of an MPI+OpenMP program.
2. Compare the performance of a mini-application with and without mpibind. 
3. Tune mpibind’s mapping for GPU-intensive codes.
-->

### 1. Determine the resources associated with tasks and threads of an MPI+OpenMP program

Using the `mpi+omp` binary to determine the compute resources to which tasks and threads are assigned on your AWS environment. For example,

```
OMP_NUM_THREADS=3 srun -p<QUEUE> -t1 -N1 -n6 mpi+omp
```

asks for six tasks on a single node, with three threads per task.

1. Run `mpi+omp` with 2 tasks and 6 threads. How many CPUs are assigned to each task? What resources are assigned to each thread? Do all threads have similar amounts of resources?
2. Run `mpi+omp` with 3 tasks and 6 threads. How has the mapping of workers to hardware changed? Do all tasks have access to similar resources? Do all threads have access to similar resources?
3. In each of the above runs, how many tasks were assigned to each NUMA domain? How many NUMA domains were assigned to each task?


### 2. Compare mappings without mpibind. 

In your AWS environment, `mpibind` is used by default. If you don't want it, you have to turn it off explicitly by adding `--mpibind=off` to your `srun` command. For example,

```
OMP_NUM_THREADS=3 srun -p<QUEUE> -t1 -N1 -n6 mpi+omp
```

uses `mpibind` and the following run does not:

```
OMP_NUM_THREADS=3 srun -p<QUEUE> -t1 -N1 -n6 --mpibind=off mpi+omp
```

Repeat the exercise above with `mpibind` set to `off`:

1. Run `mpi+omp` with 2 tasks and 6 threads and without `mpibind`. How many CPUs are assigned to each task? What resources are assigned to each thread?
2. Run `mpi+omp` with 3 tasks and 6 threads and without `mpibind`. How has the number of CPUs per task changed? How has the resource set for each thread changed?
3. How many tasks were assigned to each NUMA domain? How many NUMA domains were assigned to each task?

### 3. Tune mpibind’s mapping for GPU-intensive codes

By default, `mpibind` prioritizes CPUs and CPU locality when mapping workers to hardware. To change `mpibind`'s focus to GPUs, the flag `MPIBIND=gpu` must be added to your `srun` command.

In your AWS environment, run

```
srun -p<QUEUE> -t1 -N1 -n3 --mpibind=off mpi+omp
```

and

```
MPIBIND=gpu srun -p<QUEUE> -t1 -N1 -n3 --mpibind=off mpi+omp
```

1. Do the mapping and distribution of compute resources to tasks change when `mpibind` is tuned for GPUs?
2. How many GPUs are associated with each NUMA domain on our AWS nodes? Is this number the same for all NUMA domains?
3. Why did the mapping/distribution of compute resources change or not change?


## References

[GitHub] https://github.com/LLNL/mpibind

[SC'20] León et al. TOSS-2020: A commodity software stack for HPC. In
*International Conference for High Performance Computing, Networking,
Storage and Analysis*. IEEE Computer Society, November
2020. 

[MEMSYS'18] León et al. Achieving transparency mapping
parallel applications: A memory hierarchy affair. In *International
Symposium on Memory Systems*, Washington, DC, October 2018. ACM.

[GTC'18] León. Mapping MPI+X applications to multi-GPU architectures:
A performance-portable approach. In *GPU Technology Conference*,
San Jose, CA, March 2018.

[MEMSYS'17] León. mpibind: A memory-centric affinity algorithm for
hybrid applications. In *International Symposium on Memory Systems*,
Washington, DC, October 2017. ACM.  

[IPDPS'16] León, et al. System noise revisited: 
  Enabling application scalability and reproducibility with SMT. In
  *International Parallel & Distributed Processing Symposium*,
  Chicago, IL, May 2016. IEEE. 

