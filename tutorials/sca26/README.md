# 5th Tutorial on Mapping and Affinity (MAP)

*Edgar A. León*<br>
Lawrence Livermore National Laboratory

<p align="center">
   <img src="../figures/mpibind-logo-final_color-h.png" width="500">
</p>


## Mapping Applications on Exascale Systems

Modern supercomputing has reached a critical juncture where raw computational power alone cannot guarantee optimal application performance. As the world’s most powerful systems—including Frontier (the first exascale computer) and El Capitan (currently the fastest supercomputer)—feature increasingly complex heterogeneous architectures with over a million cores, tens of thousands of accelerators, and intricate memory hierarchies, the challenge of efficiently mapping applications to hardware has become paramount.

This half-day tutorial addresses the often-overlooked third pillar of high-performance computing: affinity—the strategic mapping of software (first pillar) to hardware resources (second pillar). While most HPC practitioners focus on algorithmic optimization and hardware capabilities, failing to account for hardware locality creates expensive data movement that can cripple even well-designed applications on top-tier systems.

## What You Will learn

* Discover and navigate complex hardware topologies using industry-standard tools like hwloc
* Master both high-level (policy-driven) and low-level (resource-specific) affinity techniques
* Control process and GPU kernel placement using Slurm and Flux resource managers
* Leverage hardware locality to minimize data movement and maximize performance
* Apply locality-aware mapping strategies that scale from small clusters to exascale systems

## Target Audience

HPC practitioners, computational scientists, students, and compute center staff seeking to bridge the gap between applications and hardware. No prior affinity experience required—just basic Linux knowledge and familiarity with parallel programming concepts.


## Requirements and Prerequisites

* Attendees will need a laptop equipped with Wi-Fi, a shell terminal,
  and the ssh program. Users will be provided accounts
  to access a supercomputer-like environment required for
  demonstrations and hands-on exercises.  
* Attendees should have a working knowledge of Linux. For
  example, they should know how to navigate a filesystem and launch
  applications from the command line.
* Attendees will also need some familiarity with high-level parallel
  programming concepts. For example, attendees should be comfortable
  with terms like thread, process, and GPU kernel, but do not need experience
  writing parallel programs.


## Schedule

<center>

| Begin | End | Topic |
|-:|-:|:-|
| 13:30 | 13:50 | Introduction and Setup |
| 13:50 | 14:30 | Module 1: Discovering the node architecture topology|
| 14:30 | 15:00 | Module 2: Process affinity with Slurm|
| *15:00* | *15:30* | *Coffee* |
| 15:30 | 16:00 | Module 3: Adding in GPUs| 
| 16:00 | 17:00 | Module 4: Process and GPU affinity with Flux| 

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

## Workbook 

Modules will be made available in Jan 2026. 

1. [Discovering the node architecture topology](module1.md)

   Attendees will identify the compute and memory components of a compute node using hwloc, a widely-used and portable tool to query the hardware topology of a computer. This includes identifying the node’s GPUs, cores, hardware threads, cache hierarchy, NUMA domains, and network interfaces. Attendees will gain a deep understanding of the architectures of both Frontier and El Captian and will be able to query their topologies in a variety of ways. Furthermore, attendees will be introduced to locality and will identify local hardware resources. Attendees will also be provided with a tool for reporting the affinity of MPI processes, OpenMP threads, and GPU kernels.


2. [Process affinity with Slurm on Frontier](module2.md)

   Attendees will learn to use Slurm, the resource manager used on most supercomputers, to map MPI processes to the hardware at runtime when submitting a job. They will learn both, low-level (resource-specification driven) and high-level (policy driven) approaches for process mapping and affinity. For example, to provide low-level affinity, they will learn how to create affinity masks to specify custom sets of CPUs and map processes to them. Finally, attendees will learn several ways to enumerate MPI processes running on the machine. All of these concepts will be demonstrated using the compute architecture of Frontier.

3. [Adding in GPUs](module3.md) 

   Building on the previous module, attendees will learn to assign GPUs to MPI processes and apply combined CPU and GPU affinity to applications without any code changes. In addition to using Slurm, they will learn to use environment variables provided by the two major GPU vendors (AMD and NVIDIA) to apply GPU affinity and will learn the importance of locality when streaming data to GPUs. Attendees will also learn to avoid conflicting directives from the different types of affinity. These concepts will also be demonstrated using Frontier’s architecture. 

4. [Process and GPU affinity with Flux](module4.md)

   In the last module, attendees will leverage all the previous teachings to discover the node architecture of El Capitan and be able to apply custom mappings of processes and GPU kernels to the hardware. Importantly, they will learn how to transfer the previous concepts of mapping and affinity into a state-of-the-art resource manger, Flux, the resource manager employed by El Capitan. This module also includes a short introduction to Flux.


## About the Instructor

Dr. Edgar A. León is a computer scientist at Lawrence Livermore National Laboratory and an IEEE senior member specializing in high-performance computing systems. He earned his Ph.D. in computer science from the University of New Mexico. León contributed to major supercomputers including Sequoia (17.5 petaflops), Sierra (125 petaflops), and El Capitan (2.7 exaflops), and previously worked at IBM Research and Sandia National Laboratories.

His research on power-aware computing, performance modeling and characterization, and runtime systems has earned multiple best paper awards, including the prestigious Karsten Schwan Award at the ACM International Symposium on High-Performance Parallel and Distributed Computing (HPDC) conference. He authored several software libraries, including mpibind, a production library that automatically maps parallel applications to heterogeneous hardware across diverse supercomputing platforms. León focuses on Exascale computing capabilities while mentoring the next generation of computer scientists.
