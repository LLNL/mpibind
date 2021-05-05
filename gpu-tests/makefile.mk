
HIP_PLATFORM = $(shell hipconfig --platform)
HWLOC_CFLAGS    = $(shell pkg-config --cflags hwloc)
HWLOC_LDLIBS    = $(shell pkg-config --libs hwloc)

PROGS  = retrieve visdevs visdevs-hwloc

all: $(PROGS)
	

ifneq ($(strip $(HAVE_AMD_GPUS)),)
retrieve: retrieve.cpp 
	hipcc -Wall -Werror -DHAVE_AMD_GPUS $< -o $@
else
retrieve: retrieve.cu 
	nvcc --Werror all-warnings -x cu $< -o $@
endif 

ifneq ($(strip $(HAVE_AMD_GPUS)),)
visdevs: visdevs.cpp 
	hipcc -Wall -Werror -DHAVE_AMD_GPUS $< -o $@
else
visdevs: visdevs.cu 
	nvcc --Werror all-warnings -x cu $< -o $@
endif

ifneq ($(strip $(HAVE_AMD_GPUS)),)
visdevs-hwloc: visdevs-hwloc.cpp 
	hipcc -Wall -Werror -DHAVE_AMD_GPUS $(HWLOC_CFLAGS) $< -o $@ $(HWLOC_LDLIBS)
else
visdevs-hwloc: visdevs-hwloc.cu 
	nvcc --Werror all-warnings $(HWLOC_CFLAGS) -x cu $< -o $@ $(HWLOC_LDLIBS)
endif


retrieve.cpp: retrieve.cu
	hipify-perl $< > $@

visdevs.cpp: visdevs.cu
	hipify-perl $< > $@

visdevs-hwloc.cpp: visdevs-hwloc.cu
	hipify-perl $< > $@

clean:
	rm -f *.o $(PROGS) $(PROGS:=.cpp)
