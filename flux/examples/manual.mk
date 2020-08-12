SOURCE_DIR = ../src

CFLAGS += -g -Wall -Werror -I$(SOURCE_DIR)
LDFLAGS += -L$(SOURCE_DIR) -lmpibind

HIPPATH = /opt/rocm-3.6.0/hip
HIPCC = $(HIPPATH)/bin/hipcc

OPENCLPATH = /opt/rocm-3.6.0/opencl/

HWLOC_LIBS = $(shell pkg-config --libs hwloc)
HWLOC_CFLAGS = $(shell pkg-config --cflags hwloc)

SO_NAME = mpibind_flux.so

all: cl_detection hip_detection omp_hello

cl_detection: opencl_detection.cpp
	$(CPP) $(CFLAGS)  \
		-I$(OPENCLPATH)/include/ -L$(OPENCLPATH)/lib/ -lOpenCL\
		opencl_detection.cpp -o $@

hip_detection: hip_detect.cpp
	$(HIPCC) $(CFLAGS)  \
		-I$(HIPPATH)/include/ -L$(HIPPATH)/lib/ \
		hip_detect.cpp -o $@

omp_hello: omp_hello.c
	$(CC) $(CFLAGS) -fopenmp omp_hello.c -o $@

debug: CFLAGS = -ggdb3 -Wall -I$(SOURCE_DIR)
debug: all

clean:
	rm -f cl_detection hip_detection omp_hello

.PHONY: clean
