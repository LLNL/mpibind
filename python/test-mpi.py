###################################################
# Edgar A. Leon
# Lawrence Livermore National Laboratory
###################################################

# There is an issue when mpi4py implicitly calls
# MPI_Finalize and loading the mpibind module,
# which leads to a segmentation fault. To avoid this,
# make sure Finalize is not called automatically.  
import mpi4py
#mpi4py.rc.threads      = True        # thread support
#mpi4py.rc.thread_level = "funneled"  # thread support level 
#mpi4py.rc.initialize   = False       # do not initialize MPI automatically
mpi4py.rc.finalize      = False       # do not finalize MPI automatically
from mpi4py import MPI

from mpibind_map import mpibind_get_mapping


###################################################
# Auxiliary functions 
###################################################

## https://stackoverflow.com/questions/6405208/\
## how-to-convert-numeric-string-ranges-to-a-list-in-python
def str2int(x):
    '''Convert a string with an integer range into a
    list of integers'''
    result = []
    for part in x.split(','):
        if '-' in part:
            a, b = part.split('-')
            a, b = int(a), int(b)
            result.extend(range(a, b + 1))
        else:
            a = int(part)
            result.append(a)
    return result

# The search path for Python modules
#import sys
#print("sys.path:")
#print(sys.path)

# Path to the MPI module 
#print("MPI module:")
#print(MPI.__file__)


###################################################
# Main 
###################################################

## Is sched_getaffinity supported?
import os                   
affinity = True if hasattr(os, 'sched_getaffinity') else False 

comm = MPI.COMM_WORLD
size = comm.Get_size()
rank = comm.Get_rank()

# if rank == 0:
#     (version, subversion) = MPI.Get_version()
#     print("Using MPI {}.{}".format(version, subversion))


# Get the mapping
# mapping["nthreads"]: The number of threads this process can launch
#     mapping["cpus"]: The CPUs assigned to this process
#     mapping["gpus"]: The GPUs assigned to this process
mapping = mpibind_get_mapping()


## Apply the CPU mapping 
if affinity:
    pid = 0
    cpus = sorted(os.sched_getaffinity(pid))
    affstr = "{:2d}/{:2d} was running on {:2d} cpus: {}\n"\
          .format(rank, size, len(cpus), cpus)

    os.sched_setaffinity(pid, str2int(mapping["cpus"]))

    cpus = sorted(os.sched_getaffinity(pid))
    print(affstr + "      now running on {:2d} cpus: {}"\
          .format(len(cpus), cpus))


