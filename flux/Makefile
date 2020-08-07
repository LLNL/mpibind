SOURCE_DIR = ../src

CFLAGS += -g -Wall -I$(SOURCE_DIR) #-Werror
LDFLAGS += -L$(SOURCE_DIR) -lmpibind

HIPPATH = /opt/rocm-3.6.0/hip
HIPCC = $(HIPPATH)/bin/hipcc

OPENCLPATH = /opt/rocm-3.6.0/opencl/

HWLOC_LIBS = $(shell pkg-config --libs hwloc)
HWLOC_CFLAGS = $(shell pkg-config --cflags hwloc)

SO_NAME = mpibind_flux.so

all: mpibind.so cldetection hippdection example

cldetection: opencl_detection.cpp
	$(CPP) $(CFLAGS)  \
		-I$(OPENCLPATH)/include/ -L$(OPENCLPATH)/lib/ -lOpenCL\
		opencl_detection.cpp -o opencl_detection

hippdection: hip_detect.cpp
	$(HIPCC) $(CFLAGS)  \
		-I$(HIPPATH)/include/ -L$(HIPPATH)/lib/ \
		hip_detect.cpp -o hip_detection

example: omp_hello.c
	$(CC) $(CFLAGS) -fopenmp omp_hello.c -o hello

mpibind.so: mpibind_plugin.c
	$(CC) $(CFLAGS) $(HWLOC_CFLAGS) -fPIC -shared -o $(SO_NAME) \
		mpibind_plugin.c $(LDFLAGS) $(HWLOC_LIBS)

debug: CFLAGS = -ggdb3 -Wall -I$(SOURCE_DIR)
debug: mpibind.so

clean:
	rm -f $(SO_NAME) detection hello hip_detection

.PHONY: clean
