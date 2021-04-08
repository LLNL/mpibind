##############################################################
# Edgar A. Leon
# Lawrence Livermore National Laboratory
##############################################################


# Check if we have AMD GPUs
#HAVE_AMD_GPUS = $(shell rocm-smi --showbus 2>/dev/null | grep GPU)
#HAVE_NVIDIA_GPUS = 1
#HAVE_AMD_GPUS    = 1

CFLAGS      = -Wall -Werror
HIP_LDFLAGS = -L$(shell hipconfig --path)/lib -lamdhip64

OBJS        = cpu.o
ifneq ($(strip $(or $(HAVE_AMD_GPUS),$(HAVE_NVIDIA_GPUS))),)
GPU_FLAGS   = -DHAVE_GPUS
OBJS       += gpu.o
endif


# Get system configuration with 'hipconfig'
# hipconfig --platform
# hipconfig --version
# hipconfig --compiler
# hipconfig --runtime

##############################################################
# Build a HIP program with nvcc (for NVIDIA hardware)
##############################################################
#	nvcc -I$(HIP_ROOT)/include $(MPI_CFLAGS) -Xcompiler -DCUDA_ENABLE_DEPRECATED -x cu $< -Xlinker -lcuda -Xlinker "$(MPI_LIBS)"
#	nvcc -I$(HIP_ROOT)/include -Xcompiler -DCUDA_ENABLE_DEPRECATED -x cu -ccbin mpicc $< -Xlinker -lcuda

##############################################################
# Build a HIP program with hipcc (for NVIDIA hardware)
# To start with a CUDA program, hipify first, e.g., 
# hipify-perl square.cu > square.cpp
# Note: hipcc takes .cpp programs (not .c for example)
##############################################################
# Export the following environment variables 
# HIP_PLATFORM=nvcc
# HIP_COMPILER=nvcc
# HIPCC_VERBOSE=1
#	hipcc -Xcompiler -DCUDA_ENABLE_DEPRECATED $(MPI_CFLAGS) $< $(MPI_LIBS) -o $@
# Could use HIP_PLATFORM to determine the flags to use
#ifeq (${HIP_PLATFORM}, nvcc)
#	HIPCC_FLAGS = -Xcompiler -DCUDA_ENABLE_DEPRECATED
#	HIPCC_FLAGS = -gencode=arch=compute_20,code=sm_20 
#endif

##############################################################
# Build an MPI program with hipcc 
##############################################################
# MPI_ROOT   = /usr/tce/packages/mvapich2/mvapich2-2.3-intel-19.0.4
# MPI_CFLAGS = -I$(MPI_ROOT)/include
# MPI_LIBS   = -L$(MPI_ROOT)/lib -lmpi
# ifneq ($(strip $(HAVE_AMD_GPUS)),)
# simple: simple.cpp
# 	hipcc $(MPI_CFLAGS) $^ $(MPI_LIBS) -o $@
# endif


##############################################################
# Link an OpenMP program with hipcc
##############################################################
# Find the OpenMP lib
# HIP_CLANG_LIB = $(shell hipconfig --hipclangpath)/../lib
# omp: omp.o gpu.o
#	hipcc -fopenmp -Xlinker -rpath=$(HIP_CLANG_LIB) $^ -o $@


## I could have chosen to build GPU programs with hipcc
## for both AMD and NVIDIA devices, but the hipcc options 
## for NVIDIA are almost like calling nvcc directly...
## I might as well call nvcc directly and no need
## for HIP on NVIDIA architectures! 


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
	mpicc $(CFLAGS) $(GPU_FLAGS) -fopenmp -c $<

cpu.o: cpu.c
	$(CC) $(CFLAGS) -c $< 

gpu.cpp: gpu.cu
	hipify-perl $< > $@


clean:
	rm -f *.o *~ $(PROGS)



# gpu-hip.o: gpu-hip.cpp affinity.h
# ifneq ($(strip $(HAVE_AMD_GPUS)),)
# 	hipcc -g -c -o $@ $<
# else
# 	nvcc -I$(HIP_ROOT)/include -Xcompiler -DCUDA_ENABLE_DEPRECATED -c -o $@ $<
# endif

#/usr/tce/packages/cuda/cuda-10.1.243/nvidia/bin/nvcc -I/usr/tce/packages/hip/hip-3.0.0/include -Xcompiler -DCUDA_ENABLE_DEPRECATED -Xcompiler -DHIP_VERSION_MAJOR=3 -Xcompiler -DHIP_VERSION_MINOR=0 -Xcompiler -DHIP_VERSION_PATCH=0 -x cu square.hipref.cpp -Xlinker '"-rpath=/usr/tce/packages/cuda/cuda-10.1.243/nvidia/lib64:/usr/tce/packages/cuda/cuda-10.1.243"'

