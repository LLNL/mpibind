
CFLAGS += -Wall -Werror
CFLAGS += $(shell pkg-config --cflags hwloc)
LDLIBS += $(shell pkg-config --libs hwloc)

PROGS = dev_tests main

OBJS = mpibind_v2.1.o

## HWLOC_XML_VERBOSE=1
## HWLOC_XMLFILE=topo-xml/lassen-hw2.xml
## ./mpibind_v0.14.1

all: $(PROGS)

main: $(OBJS)

$(OBJS): mpibind.h

clean:
	rm -f $(PROGS) *.o *~
