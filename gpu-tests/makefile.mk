
HIP_PLATFORM = $(shell hipconfig --platform)

PROGS  = retrieve


all: $(PROGS)

ifneq ($(strip $(HAVE_AMD_GPUS)),)
retrieve: retrieve.cpp 
	hipcc -Wall -Werror -DHAVE_AMD_GPUS $< -o $@
else
retrieve: retrieve.cu 
	nvcc --Werror all-warnings -x cu $< -o $@
endif 

retrieve.cpp: retrieve.cu
	hipify-perl $< > $@


clean:
	rm -f *.o $(PROGS) retrieve.cpp
