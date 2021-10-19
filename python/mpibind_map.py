###################################################
# Edgar A. Leon
# Lawrence Livermore National Laboratory
#
# This is a wrapper of mpibind functions to easily
# get an application's mapping to the hardware
# in the context of MPI.
# 
# This wrapper calls mpibind once per compute node
# so that the hardware topology is discovered once
# rather than n times, where n is the number of
# processes per node. 
#
###################################################


## Todo: Add a variable number of parameters to this
## function and pass them to MpibindHandle(). 
def mpibind_get_mapping(verbose=False):
    '''Get the mpibind mapping of an MPI program. 
    The return value is a dictionary with the keys
    nthreads, cpus, and gpus.'''
    from mpi4py import MPI
    import mpibind
    import re
    
    comm = MPI.COMM_WORLD
    size = comm.Get_size()
    rank = comm.Get_rank()
    name = MPI.Get_processor_name()

    ## Get a leader for each compute node
    match = re.search('\d+', name)
    if not match:
        print("mpibind: Could not determine node id")
        return None 

    nodeid = int(match.group())
    node_comm = comm.Split(color=nodeid, key=rank)
    node_rank = node_comm.Get_rank()
    node_size = node_comm.Get_size()

    ## One task per node calculates the mapping.
    ## This is not a hard requirement, but it is
    ## more efficient than every process discovering
    ## the topology of the compute node. 
    if node_rank == 0: 
        # Create an mpibind handle, 'ntasks' is a required parameter
        # See 'help(mpibind.MpibindHandle)' for detailed usage
        handle = mpibind.MpibindHandle(ntasks=node_size)
        
        # Create the mapping 
        handle.mpibind()
        #handle.mapping_print()

        # Distribute the mapping
        nthreads = handle.nthreads
        cpus = handle.get_cpus_ptask(0)
        gpus = handle.get_gpus_ptask(0)
        for i in range(1, node_size):
            node_comm.send(handle.get_cpus_ptask(i), dest=i)
            node_comm.send(handle.get_gpus_ptask(i), dest=i)
    else:
        nthreads = None
        cpus = node_comm.recv(source=0)
        gpus = node_comm.recv(source=0)
    
    nthreads = node_comm.scatter(nthreads, root=0)

    if verbose: 
        print('{} task {}/{}: lrank {}/{} nths {} gpus {} cpus {}'\
              .format(name, rank, size, node_rank, node_size,
                      nthreads, gpus, cpus))

    return {"nthreads": nthreads, "cpus": cpus, "gpus": gpus}



