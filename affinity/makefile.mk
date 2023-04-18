##############################################################
# Edgar A. Leon
# Lawrence Livermore National Laboratory
##############################################################

# To build with GPU support use the following: 
# HAVE_NVIDIA_GPUS = 1
# HAVE_AMD_GPUS    = 1


CFLAGS      = -Wall -Werror
HIP_LDFLAGS = -L$(shell hipconfig --path)/lib -lamdhip64

OBJS        = cpu.o
ifneq ($(strip $(or $(HAVE_AMD_GPUS),$(HAVE_NVIDIA_GPUS))),)
GPU_FLAGS   = -DHAVE_GPUS
OBJS       += gpu.o
endif


PROGS = mpi omp mpi+omp

all: $(PROGS)


mpi: mpi.o $(OBJS)
ifneq ($(strip $(HAVE_AMD_GPUS)),)
	mpicc $^ -o $@ $(HIP_LDFLAGS)
else ifneq ($(strip $(HAVE_NVIDIA_GPUS)),)
	nvcc -ccbin mpicc -Xlinker -lcuda $^ -o $@
else
	mpicc $^ -o $@ 
endif 

omp: omp.o $(OBJS)
ifneq ($(strip $(HAVE_AMD_GPUS)),)
	$(CC) -fopenmp $^ -o $@ $(HIP_LDFLAGS)
else ifneq ($(strip $(HAVE_NVIDIA_GPUS)),)
	nvcc $^ -Xcompiler -fopenmp -o $@
else
	$(CC) -fopenmp $^ -o $@
endif

mpi+omp: mpi+omp.o $(OBJS)
ifneq ($(strip $(HAVE_AMD_GPUS)),)
	mpicc -fopenmp $^ -o $@ $(HIP_LDFLAGS)
else ifneq ($(strip $(HAVE_NVIDIA_GPUS)),)
	nvcc -ccbin mpicc -Xcompiler -fopenmp -Xlinker -lcuda $^ -o $@
else
	mpicc -fopenmp $^ -o $@ 
endif 

ifneq ($(strip $(HAVE_AMD_GPUS)),)
gpu.o: gpu.cpp affinity.h
	hipcc -c $<
else
gpu.o: gpu.cu affinity.h
	nvcc --Werror all-warnings -x cu -c $<
endif 

omp.o: omp.c affinity.h
	$(CC) $(CFLAGS) $(GPU_FLAGS) -fopenmp -c $<

mpi.o: mpi.c affinity.h
	mpicc $(CFLAGS) $(GPU_FLAGS) -c $<

mpi+omp.o: mpi+omp.c affinity.h
#	mpicc $(CFLAGS) $(GPU_FLAGS) -fopenmp -c $<
	mpicc -Wall $(GPU_FLAGS) -fopenmp -c $<

cpu.o: cpu.c
	$(CC) $(CFLAGS) -c $< 

gpu.cpp: gpu.cu
	hipify-perl $< > $@


clean:
	rm -f *.o *~ $(PROGS) gpu.cpp



