# Supercomputing Foundations 

*Edgar A. León*, Lawrence Livermore National Laboratory<br>
*Jane E. Herriman*, Lawrence Livermore National Laboratory<br>
*Fernando Posada-Correa*, Oak Ridge National Laboratory<br>
*Suzanne Parete-Koon*, Oak Ridge National Laboratory<br>

## Description

This workshop provides an interactive introduction to High-Performance Computing (HPC), focusing on foundational concepts, practical applications, and hands-on exercises. Attendees will explore the basics of supercomputers, learn essential Linux skills, and gain experience with parallel programming using MPI and OpenMP. Through guided exercises, participants will build and run serial and parallel programs, analyze differences in execution, and understand how to manage jobs in an HPC environment.

The workshop also introduces advanced topics, including node architecture and topology, where attendees will learn to identify compute and memory components, including GPUs, cores, cache hierarchies, memory domains, and network interfaces. Participants will develop skills to map applications efficiently to hardware resources using tools like hwloc, emphasizing locality and affinity masks.

Building on this foundation, attendees will explore techniques for running and mapping parallel programs using Slurm, including constructing CPU-based bindings and assigning GPUs to MPI processes. The workshop highlights strategies for managing CPU and GPU affinity concurrently to optimize resource utilization and minimize data movement.

Hands-on exercises conducted in an AWS cluster environment ensure practical exposure to real-world HPC systems, enabling attendees to apply learned concepts directly. This workshop is ideal for individuals seeking to learn about HPC and its applications in scientific computing.



## Requirements

* Attendees will need a laptop equipped with Wi-Fi, a shell terminal,
  and the ssh program. Users will be provided accounts
  to access a cluster environment required for
  demonstrations and hands-on exercises.

<!--
* Attendees should have a working knowledge of Unix-like systems. For
  example, they should know how to navigate a filesystem and launch
  applications from the command line.
  
* Attendees will also need some familiarity with high-level parallel
  programming concepts. For example, attendees should be comfortable
  with terms like thread, process, and GPU, but do not need experience
  writing parallel programs.
-->


## Schedule

<center>

| Begin | End | Topic |
|-:|-:|:-|
| 13:30 | 13:50 | Introduction to Supercomputing |
| 13:50 | 15:20 | Module 1: Linux + Parallel Programming |
| *15:20* | *15:35* | *Break* |
| 15:35 | 16:55 | Module 2: Computing Architecture and Topology |
| *16:55* | *17:10* | *Break* |
| 17:10 | 18:30 | Module 3: Mapping Applications to the Hardware |

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


1. [Linux + Parallel Programming](module1.md)

1. [Computing Architecture and Topology](module2.md)

   Learn how to identify the compute and memory components of a
   compute node using `hwloc`. A precise understanding of the hardware
   resources is needed to map an application to the machine
   efficiently. This includes identifying the node's GPUs, cores,
   hardware threads, cache hierarchy, NUMA domains, and network
   interfaces. Furthermore, attendees will be introduced to locality,
   will identify local hardware resources, and will select resources
   using affinity masks.  

1. [Mapping Applications to the Hardware](module3.md)

   Learn how to use the resource manager to map a parallel
   program to the
   hardware at runtime when submitting a job. Attendees will learn to
   construct CPU-based mappings using low-level and high-level
   abstractions as well as GPU-based mappings. Attendees will learn to
   manage CPU and GPU affinity concurrently to take advantage of local
   resources and reduce data movement.






