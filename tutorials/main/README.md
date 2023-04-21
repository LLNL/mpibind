# Supercomputer Affinity 

*Edgar A. León* and *Jane E. Herriman*<br>
Lawrence Livermore National Laboratory

## Tutorial Notebook 

<br>
<p align="center">
   <img src="../figures/sierra.png" width="750"/>
</p>


1. Making sense of affinity: [Discovering the node architecture topology](module1.md)

   Learn how to identify the compute and memory components of a compute node using `hwloc` before learning how to leverage these resources to improve program performance. This includes identifying the node's GPUs, cores, hardware threads, cache hierarchy, NUMA domains, and network interfaces. Furthermore, attendees will be introduced to locality and will identify local hardware resources.

2. Applying automatic affinity: [mpibind](module2.md)

   Learn how to map parallel codes to the hardware automatically using `mpibind`. Attendees will learn to turn mpibind on and off and to identify the resources available to processes and threads in eacy case. They will explore locality effects and learn to tune mpibind to best leverage locality for hybrid applications that are either CPU or GPU constrained.

3. Exerting resource manger affinity: [Process affinity with Slurm](module3.md)

   Learn how to use Slurm’s affinity to map a program to the hardware at runtime when submitting a job. Attendees will learn low-level and policy-based binding, e.g., compute-bound, before covering task distribution enumerations. Finally, they will learn how to create affinity masks to specify sets of CPUs.

4. Exerting thread affinity: [OpenMP](module4.md)

   Learn how to map OpenMP threads to specific hardware resources. Attendees will learn how to map threads explicitly and implicitly using OpenMP’s predefined policies.

5. Putting it all together: [Adding in GPUs](module5.md)

   Learn how to assign GPUs to MPI processes and then to OpenMP threads. Learn how to apply combined process, thread, and GPU affinity policies to hybrid applications. Attendees will learn to avoid conflicting directives from the different types of affinity. Furthermore, they will assess whether automatic affinity policies may be sufficient for their use cases.

