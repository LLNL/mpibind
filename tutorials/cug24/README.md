# Supercomputer Affinity on HPE Systems
## CUG 2024

*Edgar A. León* and *Jane E. Herriman*<br>
Lawrence Livermore National Laboratory

## Schedule

<center>

| Begin | End | Topic |
|-:|-:|:-|
| 8:30 | 8:50 | Introduction + Setup |
| 8:50 | 9:20 | Module 1: Architecture Topology |
| 9:20 | 9:50 | Module 2: Process Affinity | 
| 9:50 | 10:00 | Module 2: Hands-on Exercises |
| *10:00* | *10:30* | *Coffee* |
| 10:30 | 10:40 | Module 2: Hands-on Exercises |
| 10:40 | 11:10 | Module 3: GPU Affinity | 
| 11:10 | 11:30 | Module 3: Hands-on exercises |
| 11:30 | 12:00 | Module 4: Flux affinity on the AMD MI300A APU |

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

## Tutorial Notebook 

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

2. [Process affinity with Slurm](module2.md)

   Learn how to use Slurm’s affinity to map a parallel program to the
   hardware at runtime when submitting a job. Attendees will learn to
   construct CPU-based bindings using low-level and high-level
   abstractions. High-level bindings are driven by hardware components
   such as Cores and Sockets. 

3. [Adding in GPUs](module3.md)

   Learn how to assign GPUs to MPI processes to leverage
   locality. Learn how to apply combined process and GPU
   affinity policies. Attendees will learn to
   manage CPU and GPU affinity concurrently to take advantage of local
   resources and reduce data movement.

4. [Process and GPU affinity with Flux](module4.md)

   Learn the basics of the Flux resource manager to launch parallel programs on a supercomputer. Attendees will learn how to apply combined process and GPU affinity policies using Flux. 


