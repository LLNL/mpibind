from mpi4py import MPI
import mpibind
import os

comm = MPI.COMM_WORLD
size = comm.Get_size()
rank = comm.Get_rank()

before_affinity = str(sorted(os.sched_getaffinity(0)))
n_threads = len(sorted(os.sched_getaffinity(0)))
print('pre-mpibind: task', rank, 'nths', n_threads, 'cpus', before_affinity)

handle = mpibind.MpibindHandle(ntasks=size, nthreads=None, smt=None,
    greedy=None, gpu_optim=None, restrict_ids=None, restrict_type=None)
handle.mpibind()
handle.apply(rank)

if rank == 0:
    handle.mapping_print()
    data = [(i+1)**2 for i in range(size)]
else:
    data = None

data = comm.scatter(data, root=0)
assert data == (rank+1)**2
