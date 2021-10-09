
import os
import mpibind

# This simple example does not use MPI, thus 
# specify my rank and total number of tasks 
rank = 2
ntasks = 4

# Is sched_getaffinity supported? 
getaffinity = True if hasattr(os, 'sched_getaffinity') else False 

if getaffinity:
    cpus = sorted(os.sched_getaffinity(0))
    print('Running on ', len(cpus), 'cpus: ', cpus)

# Create a handle
# Num tasks is a required parameter
handle = mpibind.MpibindHandle(ntasks)

# Create the mapping 
handle.mpibind()

# Print the mapping
handle.mapping_print()

# Apply the mapping as if I am worker 'rank'
# This function is not supported on some platforms
if getaffinity:
    handle.apply(rank)
    cpus = sorted(os.sched_getaffinity(0))
    print('Running on ', len(cpus), 'cpus: ', cpus)

