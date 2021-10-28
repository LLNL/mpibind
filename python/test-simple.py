###################################################
# Edgar A. Leon
# Lawrence Livermore National Laboratory
###################################################

import os
import mpibind

# This simple example does not use MPI, thus 
# specify my rank and total number of tasks 
rank = 2
ntasks_per_node = 4

# Is sched_getaffinity supported? 
affinity = True if hasattr(os, 'sched_getaffinity') else False 

if affinity:
    cpus = sorted(os.sched_getaffinity(0))
    affstr  = "\n>Before\n"
    affstr += "{}: Running on {:2d} cpus: {}\n"\
              .format(rank, len(cpus), cpus)

# Create a handle
# Num tasks is a required parameter
handle = mpibind.MpibindHandle(ntasks=ntasks_per_node)

# Create the mapping 
handle.mpibind()

# Print the mapping
handle.mapping_print()

# Apply the mapping as if I am worker 'rank'
# This function is not supported on some platforms
if affinity:
    handle.apply(rank)
    cpus = sorted(os.sched_getaffinity(0))
    print(affstr + ">After\n" + 
        "{}: Running on {:2d} cpus: {}"\
          .format(rank, len(cpus), cpus))

