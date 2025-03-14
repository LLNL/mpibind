# 4th Tutorial on Mapping and Affinity (MAP)

*Edgar A. León*<br>
Lawrence Livermore National Laboratory

## Bridging Applications and Hardware

When we consider the grand challenges addressed by distributed
systems, we likely imagine large-scale machines running parallel
code. Yet, these two pillars of computing – hardware and software –
are not enough to ensure high efficiency and reproducible
performance. When unaware of the topology of the underlying hardware,
even well-designed applications and research software can fail to
achieve their scientific goals. Affinity – how software maps to and
leverages local hardware resources – forms a third pillar critical to
computing systems. 

Multiple factors motivate an understanding of affinity for parallel-
and distributed-computing users. On the software side, applications
are increasingly memory-bandwidth limited making locality more
important. On the hardware side, today’s computer architectures offer
increasingly complex memory and compute topologies, making proper
affinity policies crucial to effective software-hardware assignments.

In this half-day tutorial, attendees will learn principles behind
effective affinity policies – like understanding the hardware topology
and the importance of memory and GPU locality. They will learn how to
control and apply these policies to create effective, locality-aware
mappings for MPI processes and GPU kernels and to ensure reproducible
performance. These techniques are relevant to both on-premise users
and those using the cloud such as AWS.  


## Requirements and Prerequisites

* Attendees will need a laptop equipped with Wi-Fi, a shell terminal,
  and the ssh program. Users will be provided accounts
  to access a supercomputer-like environment required for
  demonstrations and hands-on exercises.
  
* Attendees should have a working knowledge of Unix-like systems. For
  example, they should know how to navigate a filesystem and launch
  applications from the command line.
  
* Attendees will also need some familiarity with high-level parallel
  programming concepts. For example, attendees should be comfortable
  with terms like thread, process, and GPU, but do not need experience
  writing parallel programs.


## Schedule

<center>

| Begin | End | Topic |
|-:|-:|:-|
| 14:00 | 14:20 | Introduction + Setup |
| 14:20 | 15:00 | Module 1: Discovering the node architecture topology|
| 15:00 | 15:20 | Module 1: Hands-on exercises |
| 15:20 | 15:30 | Module 2: Mapping processes to the hardware |
| *15:30* | *16:00* | *Coffee* |
| 16:00 | 16:30 | Module 2: Mapping processes to the hardware (cont.)|
| 16:30 | 16:50 | Module 2: Hands-on exercises |
| 16:50 | 17:30 | Module 3: Adding in GPU kernels: Putting it all together | 
| 17:30 | 17:45 | Module 3: Hands-on exercises (optional)|

</center>

<!--
## AWS Cluster

Accounts: `user5`, `user6`, ..., `user35`

Password: 

```
ssh user5@

source /home/tutorial/scripts/user-env.sh

srun -N1 -n1 mpi
```
-->

## Notebook 

<br>
<p align="center">
   <img src="../figures/sierra.png" width="750"/>
</p>


1. [Discovering the node architecture topology](module1.md)

   Learn how to identify the compute and memory components of a
   compute node using `hwloc`. A precise understanding of the hardware
   resources is needed to map an application to the machine
   efficiently. This includes identifying the node's GPUs, cores,
   hardware threads, cache hierarchy, NUMA domains, and network
   interfaces. Furthermore, attendees will be introduced to locality,
   will identify local hardware resources, and will select resources
   using affinity masks.  

2. [Mapping processes to the hardware](module2.md)

   Learn how to use the resource manager to map a parallel
   program to the
   hardware at runtime when submitting a job. Attendees will learn to
   construct CPU-based bindings using low-level and high-level
   abstractions. High-level bindings are driven by hardware components
   such as Cores and Sockets. Furthermore, attendees will learn how to
   report affinity on a given system. 

3. [Adding in GPU kernels: Putting it all together](module3.md) 

   Learn how to assign GPUs to MPI processes to leverage
   locality. Learn how to apply combined process and GPU
   affinity policies. Attendees will learn to
   manage CPU and GPU affinity concurrently to take advantage of local
   resources and reduce data movement.



