# Module 2: Process affinity with Slurm

*Edgar A. León* and *Jane E. Herriman*<br>
Lawrence Livermore National Laboratory

## Table of contents

1. Making sense of affinity: [Discovering the node architecture topology](module1.md)
2. Exerting resource manger affinity: Process affinity with Slurm
   1. [Learning objectives](#learning-objectives)
   1. [Affinity through the resource manager](#affinity-through-the-resource-manager)
   1. [Example architectures](#example-architectures)
   1. [Task distribution across nodes](#task-distribution-across-nodes)
   1. [Simple (implicit) binding](#simple-implicit-binding)
   1. [More elaborate (explicit) binding](#more-elaborate-explicit-binding)
   1. [Multiple concurrent jobs](#multiple-concurrent-jobs)
   1. [Portability across systems](#portability-across-systems)
   1. [Extra hands-on exercises](#extra-hands-on-exercises)
   1. [References](#references)
3. Putting it all together: [Adding in GPUs](module3.md)




## Learning objectives

* Understand the benefits of applying affinity at the resource-manager
  level compared to the MPI-library level. 
* Learn how to use Slurm’s affinity to map a program to the hardware at runtime when submitting a job.
* Learn how to distribute tasks across a node in blocks and cyclically with Slurm.
* Learn how to use *implicit* binding in Slurm to map tasks to sockets, cores, and threads.
* Learn how to bind tasks to particular compute resources *explicitly* to create custom mappings. 


<!--
* Learn how to bind tasks to heterogenous sets of compute resources
* Learn implicit and explicit binding controls. 
* Learn task distribution and enumerations.
-->


## Affinity through the resource manager

In HPC environments, the resource manager controls, manages, and assigns computational resources to user
jobs. The resource manager both assigns compute nodes to jobs and on-node resources to the job's tasks.
*Resource managers are also a way to provide affinity.*

A key benefit of applying affinity through the resource manager is
that this approach works across MPI libraries. We could instead handle affinity at the MPI-library level, but because each MPI library has its own interface and policies that provide affinity, this is not a portable solution when we switch from one MPI library to another.
For this reason, we prefer to handle affinity at the resource manager level
rather than at the MPI-library level.

In this module, we focus on the **Slurm** resource manager and delve into
three aspects:
1. Distribution of MPI tasks among nodes and within a node.
1. User-friendly bindings using high-level abstractions.
1. Binding using low-level abstractions, which provide the most
flexibility.

Before diving into the technical details, let's agree on some
terminology:

- When a user submits a job, the resource manager will
allocate a set of compute nodes to run the job; we call these nodes
the `node allocation`. 
- Within this allocation a user may run a single
job or multiple jobs; Slurm refers to these as `job steps`, but in
this module, unless specified otherwise, we will simply call them
`jobs`. 
- In addition, to be consistent with Linux, we use the term `CPU` to
refer to a hardware thread.


## Example architectures

We'll be using a couple machines in the examples below, whose topologies are depicted [here](../common/archs.md). 


<!--
Focusing on `cpu-bind`
leave gpu-bind for GPU affinity section
-->

<!--
Slurm configuration 
 - {src: 'files/mpibind14.2_prolog', dest:
 '/etc/slurm/mpibind.prolog', mode: '0555'}

/etc/slurm/slurm.conf.common
/etc/slurm/slurm.conf
/etc/slurm/mpibind.prolog

$ grep -i mpibind slurm.conf*
slurm.conf:TaskProlog=/etc/slurm/mpibind.prolog
slurm.conf.common:InteractiveStepOptions="--interactive --preserve-env --pty --mpibind=off $SHELL"
--> 

## Task distribution across nodes 

<!--
Ordering - How placed processes are assigned task IDs
-->

An MPI job is comprised of tasks, each having a unique ID called
*rank*. The placement of tasks and rank assignments determines which
ranks are associated with a particular compute node. 

To control placement and ordering, Slurm provides the `--distribution`
option. In this tutorial, we focus on a simplified version of this
option for clarity and ease of explanation:

<!--
```
--distribution=<value>[:<value>][:<value>][,pack]
```
This option specifies the ordering of tasks across compute nodes
and the distribution of the node resources among tasks for binding.
The first distribution method (before the first ":") controls the
ordering of tasks across nodes. The second distribution method (after
the first ":") controls the distribution of CPUs across sockets. The
third distribution method (after the second ":") controls 
the distribution of CPUs across cores. The second and third
methods apply only if task affinity is enabled (we will cover
this later). 
-->

```
--distribution=<value>[,Pack|NoPack]
```
This option controls the placement and ordering of tasks across
compute nodes.
* The `block` method distributes consecutive tasks
to the same node when possible.
* The `cyclic` method distributes 
consecutive tasks to consecutive nodes in a round-robin manner.

The `nopack` option distributes the tasks among nodes evenly, while
the `pack` option packs as many tasks as possible on one node before using
the next one, with the exception that all nodes will receive at least one task. The pack option applies to the block distribution method 
only. 

To understand these methods, let us try a few examples. 

### Example 1. Block ordering

Below we see that `hostname` is run on 3 nodes and 5 tasks, blockwise. `nopack` isn't specified here, but it is the default, so 5 tasks are distributed across 3 nodes as evenly as possible. The first two tasks are assigned to `tioga38`, the next two tasks are assigned to `tioga39`, and only the final task remains for `tioga40`.


<details>
<summary>

```
# Block ordering
# Contiguous tasks on the same node
$ srun -N3 -n5 -l --distribution=block hostname
```
</summary>

```
0: tioga38
1: tioga38
2: tioga39
3: tioga39
4: tioga40
```
</details>


### Example 2. Cyclic ordering

Below we see that `hostname` is run on 3 nodes and 5 tasks with cyclic assignments. `nopack` is again the default. So, again we see that the first two nodes -- `tioga38` and `tioga39` -- each receive 2 tasks and the third node `tioga40` receives only a single task. This time, however, each node is assigned a single task before any node receives a second task; consecutive tasks are never assigned to the same node.


<details>
<summary>

```
# Cyclic ordering
# Round robin contigous tasks on consecutive nodes
$ srun -N3 -n5 -l --distribution=cyclic hostname 
```
</summary>

```
0: tioga38
1: tioga39
2: tioga40
3: tioga38
4: tioga39
```
</details>


### Example 3. Default method

Below we see that we get the same behavior as in `Example 1` with or without explicitly specifying `--distribution=block` because `block` distribution is the default.


<details>
<summary>

```
# The block method is the default 
$ srun -N3 -n5 -l hostname
```
</summary>

```
1: tioga38
0: tioga38
4: tioga40
2: tioga39
3: tioga39
```
</details>

### Example 4. The `pack` option

Finally, we see how the behavior from `Example 1` changes for the `pack` option rather than the default `nopack`. This time, three consecutive tasks are assigned to the first node. Each of the other nodes receives a single task.


<details>
<summary>

```
# Pack the tasks onto the allocated nodes 
$ srun -N3 -n5 -l --distribution=block,pack hostname
```
</summary>

```
0: tioga38
1: tioga38
2: tioga38
3: tioga39
4: tioga40
```
</details>



**Comprehension check:** *What would the distribution and numbering of tasks look like if we updated `Example 4` to have 10 tasks and 3 nodes?*


## Simple (implicit) binding 

Once the tasks have been distributed and ordered among compute nodes,
the next natural step is to assign on-node resources to each task. For
this step we will leverage the following options:

```
-c, --cpus-per-task=<ncpus>
--cpu-bind=none|threads|cores|sockets|rank
```

Let's review a few examples.

### Example 5. Binding by rank

With `--cpu-bind=rank`, you can cause each task to bind to the CPU with the same number as the task's rank. For example, `Task 2` binds to CPU 2 and `Task 3` to CPU 3.

<details>
<summary>

```
# For each task with rank 'k', bind task to CPU 'k'
$ srun -N1 -n4 --cpu-bind=rank mpi 
```
</summary>

```
tioga12    Task   0/  4 running on 1 CPUs: 0
tioga12    Task   2/  4 running on 1 CPUs: 2
tioga12    Task   3/  4 running on 1 CPUs: 3
tioga12    Task   1/  4 running on 1 CPUs: 1
```
</details>

#### Hands-on exercise A: Binding by rank

On your AWS desktop, run

```
srun -N1 -n16 -t1 --cpu-bind=rank mpi
```

```
srun -N1 -n48 -t1 --cpu-bind=rank mpi
```

and then

```
srun -N1 -n49 -t1 --cpu-bind=rank mpi
```

Do they all work?

### Example 6. Binding to threads

When `-c` is used with `--cpu-bind`, `-c` specifies the number of objects to be bound to each task and `-cpu-bind` specifies the type of object. 

We see that below, where `--cpu-bind=thread` specifies thread as the object considered by `-c`. `-c6` therefore specifies that each task should be assigned to 6 threads. Contrast this with the behavior in the next example.

<details>
<summary>

```
# Bind each task to 6 hardware threads 
$ srun -N1 -n4 -c6 --cpu-bind=thread mpi  
```
</summary>

```
tioga12    Task   3/  4 running on 6 CPUs: 18-23
tioga12    Task   0/  4 running on 6 CPUs: 0-5
tioga12    Task   1/  4 running on 6 CPUs: 6-11
tioga12    Task   2/  4 running on 6 CPUs: 12-17
```
</details>


### Example 7. Binding to cores

As in the example above, `-c6` means that each task will be assigned to 6 objects. Here, `--cpu-bind=core` specifies `core` as the object type, so each task will be assigned to 6 cores.

On `Tioga`, each core has two hardware threads, so each task will be assigned to 12 threads. For example, Task 3 is assigned to the threads `18-23` and `82-87`.

<details>
<summary>

```
# Bind each task to 6 cores
# Remember that each core is comprised of 2 hardware threads
# For example, the first core has CPUs 0,48
$ srun -N1 -n4 -c6 --cpu-bind=core mpi
```
</summary>

```
tioga12    Task   0/  4 running on 12 CPUs: 0-5,64-69
tioga12    Task   1/  4 running on 12 CPUs: 6-11,70-75
tioga12    Task   3/  4 running on 12 CPUs: 18-23,82-87
tioga12    Task   2/  4 running on 12 CPUs: 12-17,76-81
```
</details>

#### Hands-on exercise B: cores vs. hardware threads

On your AWS desktop, try

```
srun -t1 -N1 -n2 -c3 --cpu-bind=core mpi
```

and then 

```
srun -t1 -N1 -n2 -c3 --cpu-bind=thread mpi
```

How many CPUs are assigned to your tasks in each case?

### Example 8. No binding

With `--cpu-bind=none`, no binding occurs and tasks are not bound to specific resources. Instead, all tasks are distributed across all threads. In this case, 4 tasks are distributed across 127 hardware threads.

<details>
<summary>

```
# No binding, everybody runs wild! 
$ srun -N1 -n4 --cpu-bind=none mpi
```
</summary>

```
tioga12    Task   1/  4 running on 128 CPUs: 0-127
tioga12    Task   2/  4 running on 128 CPUs: 0-127
tioga12    Task   3/  4 running on 128 CPUs: 0-127
tioga12    Task   0/  4 running on 128 CPUs: 0-127
```
</details>



<!--
Request <ncpus> for each process
--threads-per-core=<threads>
In task layout, use the specified maximum number of threads per core
(default is 1; there are 2 hardware threads per physical CPU core).
--> 

<!--
  Policy based binding 
  --hint=<type>
  type: compute_bound, memory_bound, [no]multithread
  Option does not work well on LC, perhaps sockets are
  not configured correctly?
--> 

## More elaborate (explicit) binding 

<!--
* Verbose option
* mask_cpu, map_cpu
* Ordinals: bind each task to an individual CPU
* Affinity masks: Bind each task to a set of CPUs
-->

More advanced use cases may require placing tasks on
specific CPUs. For example, one may want a monitoring daemon running
on the *system cores* or, say, the last core of the second socket. To
do this explicit binding, Slurm provides the following parameters to
an already familiar option:

```
--cpu-bind=map_cpu:<list>
--cpu-bind=mask_cpu:<list>
```

With `map_cpu` one specifies a 1:1 mapping of tasks to
CPUs, one CPU per task. The first task is placed on the first CPU in
the list and so on. With `mask_cpu` one specifies a 1:1 mapping of
tasks to sets of CPUs, one CPU set per task. The first task is placed
on the first CPU set in the list and so on. A set of CPUs is
specified with a CPU mask.

While these options provide the flexibility to implement custom
mappings, the downside is the user must know the topology to determine
the CPU ids of the resources of interest. Thus, this is not a portable
solution across systems and architectures.

We will resort to more examples for ease of exposition. 


### Example 9. `map_cpu`

In this first example with `map_cpu`, we want to use the first two cores of each NUMA domain. For example, cores `0,1` on NUMA #0 will be assigned to tasks `0-1` and cores `32,33` from NUMA #2 will be assigned to tasks `4-5`:


<details>
<summary>

```
# Place tasks 0-1 on the first two cores of the first NUMA
# Place tasks 2-3 on the first two cores of the second NUMA
# Place tasks 4-5 on the first two cores of the third NUMA
# Place tasks 6-7 on the first two cores of the fourth NUMA
$ srun -N1 -n8 --cpu-bind=map_cpu:0,1,16,17,32,33,48,49 mpi
```
</summary>

```
tioga12    Task   6/  8 running on 1 CPUs: 48
tioga12    Task   7/  8 running on 1 CPUs: 49
tioga12    Task   0/  8 running on 1 CPUs: 0
tioga12    Task   1/  8 running on 1 CPUs: 1
tioga12    Task   2/  8 running on 1 CPUs: 16
tioga12    Task   3/  8 running on 1 CPUs: 17
tioga12    Task   4/  8 running on 1 CPUs: 32
tioga12    Task   5/  8 running on 1 CPUs: 33
```
</details>


Notice that in the above example we merely specified the set of CPUs to use and the number of tasks; the ordering of the cores in the list passed to `--cpu-bind=map_cpu:` determined the exact assignment of tasks to cores.

#### Hands-on exercise C: choosing your resources

On AWS, try running

```
srun -t1 -N1 -n4 --cpu-bind=map_cpu:7,5,10,6 mpi
```

```
srun -t1 -N1 -n3 --cpu-bind=map_cpu:7,5,10,6 mpi
```

```
srun -t1 -N1 -n8 --cpu-bind=map_cpu:7,5,10,6 mpi
```

Do you see the mappings you expected? Which cores are used for `-n3` and `-n8`?

### Example 10. `map_cpu`

As in the last example, we'll assign 8 tasks to the same 8 cores. This time, we'll manually assign these tasks in a round robin fashion between the two sockets by changing the order of the list of cores passed to `--cpu-bind=map_cpu`. This means that tasks are assigned in the order of the NUMA domains, but that each NUMA domain receives a first task before the first NUMA domain receives a second task.

<details>
<summary>

```
# Round robin the tasks between the two sockets
$ srun -N1 -n8 --cpu-bind=map_cpu:0,16,32,48,1,17,33,49 mpi
```
</summary>

```
tioga12    Task   0/  8 running on 1 CPUs: 0
tioga12    Task   1/  8 running on 1 CPUs: 16
tioga12    Task   2/  8 running on 1 CPUs: 32
tioga12    Task   3/  8 running on 1 CPUs: 48
tioga12    Task   4/  8 running on 1 CPUs: 1
tioga12    Task   5/  8 running on 1 CPUs: 17
tioga12    Task   6/  8 running on 1 CPUs: 33
tioga12    Task   7/  8 running on 1 CPUs: 49
```
</details>


### Example 11. `mask_cpu`

The `map_cpu` parameter is limited to binding a task to a single
CPU. To map a task to multiple CPUs, we use the `mask_cpu`
parameter. As its name implies, one needs to specify a CPU mask and,
although one can calculate a mask *by hand*, we will use `hwloc-calc`,
which you learned in [Module 1](module1.md). 

In the previous examples, we mapped a task to a core, but because of
the map_cpu limitation we were not able to place the task on both CPUs
comprising that core. With mask_cpu, we can do that.

First, let's calculate the CPU masks for the first three cores of each NUMA domain:

```
$ hwloc-calc --taskset --physical-output NUMAnode:0.core:0-2
0x70000000000000007

$ hwloc-calc --taskset --physical-output NUMAnode:1.core:0-2
0x700000000000000070000

$ hwloc-calc --taskset --physical-output NUMAnode:2.core:0-2
0x7000000000000000700000000

$ hwloc-calc --taskset --physical-output NUMAnode:3.core:0-2
0x70000000000000007000000000000
```

Now, let's check the masks are correct:

<details>
<summary>

```
$ srun -N1 -n1 --cpu-bind=mask_cpu:0x70000000000000007 mpi
```
</summary>

```
tioga12    Task   0/  1 running on 6 CPUs: 0-2,64-66
```
</details>

<details>
<summary>

```
$ srun -N1 -n1 --cpu-bind=mask_cpu:0x700000000000000070000 mpi
```
</summary>

```
tioga12    Task   0/  1 running on 6 CPUs: 16-18,80-82
```
</details>


<details>
<summary>

```
$ srun -N1 -n1 --cpu-bind=mask_cpu:0x7000000000000000700000000 mpi
```
</summary>

```
tioga12    Task   0/  1 running on 6 CPUs: 32-34,96-98
```
</details>


<details>
<summary>

```
$ srun -N1 -n1 --cpu-bind=mask_cpu:0x70000000000000007000000000000 mpi
```
</summary>

```
tioga13    Task   0/  1 running on 6 CPUs: 48-50,112-114
```
</details>


Not only we got the right cores, but we also got both hardware
threads of each core, as we wanted.

Finally, in a single command, let's use these masks to map four processes (one per NUMA) to the first three cores on each NUMA node. 

<details>
<summary>

```
$ srun -N1 -n4 --cpu-bind=mask_cpu:0x70000000000000007,0x700000000000000070000,0x7000000000000000700000000,0x70000000000000007000000000000 mpi
```
</summary>

```
tioga12    Task   0/  4 running on 6 CPUs: 0-2,64-66
tioga12    Task   3/  4 running on 6 CPUs: 48-50,112-114
tioga12    Task   1/  4 running on 6 CPUs: 16-18,80-82
tioga12    Task   2/  4 running on 6 CPUs: 32-34,96-98
```
</details>

#### Hands-on exercise D: Creating masks

On AWS, create a mask via

```
srun -t1 -N1 -n1 hwloc-calc --taskset --physical-output NUMAnode:0.core:4-8
```

Check your work by using the mask created in the following:

```
srun -t1 -N1 -n1 hwloc-calc <MASK> --intersect core
```

#### Hands-on exercise E: Using masks to bind tasks

On AWS, use your mask from exercise D to bind two tasks to this set of cores by running

```
srun -N1 -n2 --cpu-bind=mask_cpu:<Mask from Ex. D> mpi
```

You should see output like:

```
gpu-st-g4dnmetal-1 Task   0/  2 running on 10 CPUs: 4-8,52-56
gpu-st-g4dnmetal-1 Task   1/  2 running on 10 CPUs: 4-8,52-56
```


## Multiple concurrent jobs 

When running multiple concurrent jobs (job steps), one has to be
careful so that jobs are not serialized and do not overlap the
resources where they are run. Slurm provides the following option to
help with that: 

```
--exclusive
```

Let's start by launching concurrent jobs within an
allocation. Note that this node allocation has been created so that
jobs initiated by other scripts or users cannot share these nodes. To launch concurrent jobs we use
`&`. `&` makes the associated command non-blocking, so that subsequent lines/commands are read before the current line finishes its work.


<details>
<summary>

```
srun --cpu-bind=thread -n4 -c4 mpi &
srun --cpu-bind=thread -n4 -c4 mpi &
wait
```
</summary>

```
tioga12    Task   1/  4 running on 4 CPUs: 4-7
tioga12    Task   2/  4 running on 4 CPUs: 8-11
tioga12    Task   3/  4 running on 4 CPUs: 12-15
tioga12    Task   0/  4 running on 4 CPUs: 0-3
srun: Job 24215 step creation temporarily disabled, retrying (Requested nodes are busy)
srun: Step created for job 24215
tioga12    Task   1/  4 running on 4 CPUs: 4-7
tioga12    Task   2/  4 running on 4 CPUs: 8-11
tioga12    Task   3/  4 running on 4 CPUs: 12-15
tioga12    Task   0/  4 running on 4 CPUs: 0-3
```
</details>


The error we see above shows that the second job is being serialized, even though we're using the `&` to allow for concurrent jobs. Consistent with that, we see that (for example) Task 1 in both jobs uses the same set of CPUs #4-7.

This serialization is happening because of the way the node allocation was created. Furthermore, the set of CPUs used by each job are the same by default, so if they were running concurrently, they would run on the same resources. We can change this behavior with the exclusive flag:

<details>
<summary>

```
srun  --exclusive --cpu-bind=thread -n4 -c4 mpi &
srun  --exclusive --cpu-bind=thread -n4 -c4 mpi &
wait
```
</summary>

```
tioga12    Task   0/  4 running on 4 CPUs: 16-19
tioga12    Task   3/  4 running on 4 CPUs: 28-31
tioga12    Task   2/  4 running on 4 CPUs: 24-27
tioga12    Task   1/  4 running on 4 CPUs: 20-23
tioga12    Task   2/  4 running on 4 CPUs: 8-11
tioga12    Task   3/  4 running on 4 CPUs: 12-15
tioga12    Task   1/  4 running on 4 CPUs: 4-7
tioga12    Task   0/  4 running on 4 CPUs: 0-3
```
</details>


Now, both jobs are running concurrently and using different
CPUs. Let's try one more example:

<details>
<summary>

```
srun  --exclusive --cpu-bind=thread -n4 -c4 mpi &
srun  --exclusive --cpu-bind=thread -n4 -c5 mpi &
wait
```
</summary>

```
tioga12    Task   2/  4 running on 5 CPUs: 26-30
tioga12    Task   3/  4 running on 5 CPUs: 31-35
tioga12    Task   0/  4 running on 5 CPUs: 16-20
tioga12    Task   1/  4 running on 5 CPUs: 21-25
tioga12    Task   1/  4 running on 4 CPUs: 4-7
tioga12    Task   2/  4 running on 4 CPUs: 8-11
tioga12    Task   3/  4 running on 4 CPUs: 12-15
tioga12    Task   0/  4 running on 4 CPUs: 0-3
```
</details>


Notice that here, the job with five CPUs per task (`-c5`) is assigned resources first and the job with four CPUs per task (`-c4`) is assigned resources second. It appears the order in which these `srun` commands was run didn't impact the order in which resources were assigned.

<!--
These examples show we can use the simple binding interface so
that Slurm figures out for us what CPUs to run on. But, we could also
use the more flexible interface to specify exactly what resources to
run on:

--cpu-bind:mask_cpu does not appear to work with --exclusive. 
-->


## Portability across systems

While some of the binding primitives provided by Slurm can be portable
across different architectures, some of them are not, because they
rely on the user's knowledge of the machine topology. Things like
CPU IDs may refer to different resources on different systems and a
valid ID on one system may not be valid on another.

On the other hand, while mpibind is portable it
does not provide a full-fledged interface for custom mappings in the
same way Slurm does. We are faced with a tradeoff between flexibility and portability. 

In a later module, we will cover a few example mappings from mpibind and how they can be constructed with Slurm. These examples will show how mpibind abstracts out the node topology with a simple interface to reach a portable mapping policy across systems. 




<!--
To be more specific, let's try to mimic a few mappings from
`mpibind` onto a couple of systems with the Slurm's binding primitives
you learned in this module.
-->


## Extra hands-on exercises

### 1.  Create an affinity mask for all the CPUs on the second NUMA domain.

1. Use `hwloc-calc` as in Example 11 to create a mask for all the CPUs on the second NUMA domain (NUMA 1, in 0-indexing) of an AWS node. 

The architecture of the login nodes might be different from the compute nodes (those in the queues). So, instead of simply running `hwloc-calc` at the command line, you want to run it inside a `srun` command. Complete

```
srun -N1 -n1 -t1 hwloc-calc <add text here>
```

<details>
<summary>

Hint:

</summary>


You'll want to replace the `<>` in the following:

```
srun -N1 -n1 -t1 hwloc-calc --taskset --physical-output NUMAnode:<NUMA ID>
```

```
srun -N1 -n1 -t1 hwloc-calc --taskset --physical-output NUMAnode:<NUMA ID>.core:<core IDs>
```

Check that you get the same mask using either syntax!

</details>


2. After creating the mask for the second socket, check that it was correct with the `--cpu-bind=mask_cpu` flag:

```
[user2@ip-10-0-0-130 ~]$ srun -N1 -t1 --cpu-bind=mask_cpu:<Mask from Ex.1> mpi
gpu-st-g4dnmetal-1 Task   0/  1 running on 48 CPUs: 24-47,72-95
```


### 2. Using explicit binding, map one task per each of the first 3 cores on the first socket and a single task using all cores on the second socket.

*Note:* Specifying the CPUs where you want to run tasks via `--cpu-bind=map_cpu` creates a one-to-one mapping of tasks to CPUs. If you want to map a single task to multiple CPUs, you will need to create a CPU mask.

1. Use `hwloc-calc` as in the above exercise to create a mask for each of the first three cores on the first NUMA domain (NUMA `0`).
2. Grab the mask that you created for the second socket from the above exercise.
3. Use the masks from steps 1 and 2 to map one task per each of the first three cores on the first socket and a single task on the second socket. Use the `--cpu-bind=mask_cpu...` flag as in Example 11. Your command should be of the form 

```
srun -N1 -n4 -t1 --cpu-bind=<edit with CPU info!> mpi
```

You want your output to look something like

```
gpu-st-g4dnmetal-1 Task   3/  4 running on 48 CPUs: 24-47,72-95
gpu-st-g4dnmetal-1 Task   0/  4 running on 2 CPUs: 0,48
gpu-st-g4dnmetal-1 Task   1/  4 running on 2 CPUs: 1,49
gpu-st-g4dnmetal-1 Task   2/  4 running on 2 CPUs: 2,50
```

### 3. Round robin 8 processes to the last two cores of each NUMA. 

1. Identify the last two cores on each NUMA of the AWS compute nodes. For example, on a node with 2 NUMA domains and 12 cores per NUMA, these would be cores `10,11,22,23`.

2. Use a command of the form

```
srun -N1 -n8 -t1 --cpu-bind=<edit with CPU info!> mpi
```

to create the following mapping for 8 tasks, assuming 2N cores on your AWS node:

Task 0: `N-2`
Task 1: `N-1`
Task 2: `2N-2`
Task 3: `2N-1`
Task 4: `N-2`
Task 5: `N-1`
Task 6: `2N-2`
Task 7: `2N-1`

<details>
<summary>

Answer:

</summary>

```
[user2@ip-10-0-0-130 ~]$ srun -N1 -n8 -t1 --cpu-bind=map_cpu:22,23,46,47 mpi
gpu-st-g4dnmetal-1 Task   0/  8 running on 1 CPUs: 22
gpu-st-g4dnmetal-1 Task   1/  8 running on 1 CPUs: 23
gpu-st-g4dnmetal-1 Task   2/  8 running on 1 CPUs: 46
gpu-st-g4dnmetal-1 Task   3/  8 running on 1 CPUs: 47
gpu-st-g4dnmetal-1 Task   4/  8 running on 1 CPUs: 22
gpu-st-g4dnmetal-1 Task   5/  8 running on 1 CPUs: 23
gpu-st-g4dnmetal-1 Task   6/  8 running on 1 CPUs: 46
gpu-st-g4dnmetal-1 Task   7/  8 running on 1 CPUs: 47
```

Output will likely be in a different order!

</details>


### 4. Run 3 concurrent tasks: one on 3 cores, one on 3 hardware threads and the last one on 5 cores. 
As in Exercise 2, create CPU masks for the following sets of non-overlapping resources:

* 3 cores
* 3 CPUs
* 5 cores

Use these masks and `srun <flags> mpi` to run one task on each set of resources.

<details>
<summary>

How to create the CPU masks:

</summary>

```
[user2@ip-10-0-0-130 ~]$ srun hwloc-calc --taskset --physical-output NUMAnode:0.PU:1-3
0x3000000000002
[user2@ip-10-0-0-130 ~]$ srun hwloc-calc --taskset --physical-output NUMAnode:0.core:21-23
0xe00000000000e00000
[user2@ip-10-0-0-130 ~]$ srun hwloc-calc --taskset --physical-output NUMAnode:1.core:1-5
0x3e00000000003e000000
```

Sanity checking:

```
[user2@ip-10-0-0-130 ~]$ srun hwloc-calc 0x3000000000002 --intersect core
0,1
[user2@ip-10-0-0-130 ~]$ srun hwloc-calc 0x3000000000002 --intersect PU
1,2,3
[user2@ip-10-0-0-130 ~]$ srun hwloc-calc 0xe00000000000e00000 --intersect core
21,22,23
[user2@ip-10-0-0-130 ~]$ srun hwloc-calc 0xe00000000000e00000 --intersect PU
42,43,44,45,46,47
[user2@ip-10-0-0-130 ~]$ srun hwloc-calc 0x3e00000000003e000000 --intersect core
25,26,27,28,29
[user2@ip-10-0-0-130 ~]$ srun hwloc-calc 0x3e00000000003e000000 --intersect PU
50,51,52,53,54,55,56,57,58,59
```

</details>



<details>
<summary>

How to run 1 task on each mask:

</summary>

```
[user2@ip-10-0-0-130 ~]$ srun -N1 -n3 --cpu-bind=mask_cpu:0x3000000000002,0xe00000000000e00000,0x3e00000000003e000000 mpi
gpu-st-g4dnmetal-1 Task   0/  3 running on 3 CPUs: 1,48-49
gpu-st-g4dnmetal-1 Task   1/  3 running on 6 CPUs: 21-23,69-71
gpu-st-g4dnmetal-1 Task   2/  3 running on 10 CPUs: 25-29,73-77
```

</details>


<!--
* Using a compute-bound policy, map an MPI application onto hardware.

Application hints (compute-bound, memory-bound, multithread)
Do these hints work on `pascal`? They did not on `corona`.
--> 

## References

* `hwloc-calc -h`

* `man 7 hwloc`

* `srun --cpu-bind=help`

* `man srun`

* [Slurm srun](https://slurm.schedmd.com/srun.html)
<!--search for gpu-->

* [Slurm support for multi-core/multi-thread architectures](https://slurm.schedmd.com/mc_support.html)


<!--
https://lc.llnl.gov/confluence/pages/viewpage.action?pageId=659292776
-->



<!--
## hwloc stuff

<table style="text-align:center;margin-left:auto;margin-right:auto;border:1.5px solid black;">
  <tr>
    <th>Memory objects</th>
    <td>NUMA domain</td>
    <td>Memory-side cache</td>
  </tr>
</table>

<table style="text-align:center;margin-left:auto;margin-right:auto;border:1.5px solid black;">
  <tr>
    <th>Normal objects</th>
    <td>Machine</td>
    <td>Package</td>
    <td>Die</td>
    <td>Core</td>
    <td>PU</td>
    <td>Cache</td>
  </tr>
</table>

<table style="text-align:center;margin-left:auto;margin-right:auto;border:1.5px solid black;">
  <tr>
    <th rowspan="2">I/O objects</th>
    <td rowspan="2">Bridge</td>
    <td rowspan="2">PCI</td>
    <td colspan="5">OS device</td>
  </tr>
  <tr>
    <td>Block</td>
    <td>Network</td>
    <td>OpenFabrics</td>
    <td>GPU</td>
    <td>CoProcessor</td>
  </tr>
</table>
-->

<!--
<table style="text-align:center;margin-left:auto;margin-right:auto;">
<table style="float: left;width: 40%">
-->

<!-- style="text-align:center" 
<table style="width:100%">
-->

<!--
<table style="text-align:center;margin-left:auto;margin-right:auto;">
  <tr>
    <th>Memory</th>
    <th>Normal</th>
    <th colspan="2">I/O</th>
  </tr>
  <tr>
    <td rowspan="4">NUMA domain</td>
    <td>Machine</td>
    <td colspan="2">Bridge</td>
  </tr>
  <tr>
    <td>Package</td>
    <td colspan="2">PCI</td>
  </tr>
  <tr>
    <td>Group</td>
    <td rowspan="5">OS device</td>
    <td>Block</td>
  </tr>
  <tr>
    <td>Die</td>
    <td>Network</td>
  </tr>
  <tr>
    <td rowspan="3">Memory-side cache</td>
    <td>Core</td>
    <td>OpenFabrics</td>
  </tr>
  <tr>
    <td>PU</td>
    <td>GPU</td>
  </tr>
  <tr>
    <td>CPU cache</td>
    <td>CoProcessor</td>
  </tr>
</table>
-->


<!--
* Memory 
  * NUMA nodes or domains
* Normal
  * Machine
  * Package
  * Die
  * Core
  * PU
  * CPU caches
* I/O
  * Bridges
  * PCI
  * OS devices
    * Hard disks or non-volatile memory (e.g., sda)
    * Network interfaces (e.g., eth0)
    * OpenFabrics HCAs (e.g., mlx5_0)
    * GPUs (e.g., rsmi0, nvml0)
    * Co-processors (e.g., cuda0, opencl0d0)
* Misc
-->

<!--
AMD ROCm SMI Library (RSMI)
NVIDIA Management Library (NVML)
NVIDIA CUDA
OpenCL 

HWLOC_OBJ_OSDEV_BLOCK
HWLOC_OBJ_OSDEV_NETWORK
HWLOC_OBJ_OSDEV_OPENFABRICS
HWLOC_OBJ_OSDEV_GPU
HWLOC_OBJ_OSDEV_COPROC
-->

<!--
Normal and Memory objects have (non-NULL) CPU sets and nodesets, while
I/O and Misc don't. 
-->

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
