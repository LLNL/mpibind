# 4th Tutorial on Mapping and Affinity (MAP)
## Bridging Applications and Hardware

<!--
## ISC 2024
-->

*Edgar A. León* and *Jane E. Herriman*<br>
Lawrence Livermore National Laboratory

## Schedule

<center>

| Begin | End | Topic |
|-:|-:|:-|
| 9:00 | 9:20 | Introduction + Setup |
| 9:20 | 10:00 | Module 1: Discovering the architecture topology |
| 10:00 | 10:20 | Module 1: Hands-on exercises | 
| 10:20 | 11:00 | Module 2: Exerting process and thread affinity |
| *11:00* | *11:30* | *Coffee* |
| 11:30 | 11:50 | Module 2: Hands-on exercises |
| 11:50 | 12:30 | Module 3: Adding in GPUs | 
| 12:30 | 13:00 | Module 3: Hands-on exercises |

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

## *Preliminary* Tutorial Notebook 

<br>
<p align="center">
   <img src="../figures/sierra.png" width="750"/>
</p>


1. Making sense of affinity: [Discovering the node architecture topology](module1.md)

   Learn how to identify the compute and memory components of a
   compute node using `hwloc`. A precise understanding of the hardware
   resources is needed to map an application to the machine
   efficiently. This includes identifying the node's GPUs, cores,
   hardware threads, cache hierarchy, NUMA domains, and network
   interfaces. Furthermore, attendees will be introduced to locality,
   will identify local hardware resources, and will select resources
   using affinity masks.  

2. Exerting resource manager affinity: [Process affinity with Slurm](module2.md)

   Learn how to use Slurm’s affinity to map a parallel program to the
   hardware at runtime when submitting a job. Attendees will learn to
   construct CPU-based bindings using low-level and high-level
   abstractions. High-level bindings are driven by hardware components
   such as Cores and Sockets. 

3. Putting it all together: [Adding in GPUs](module3.md)

   Learn how to assign GPUs to MPI processes to leverage
   locality. Learn how to apply combined process and GPU
   affinity policies. Attendees will learn to
   manage CPU and GPU affinity concurrently to take advantage of local
   resources and reduce data movement.



