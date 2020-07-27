
UNAME           = $(shell uname)
BASIC_CFLAGS    = -Wall -Werror

HWLOC_CFLAGS    = $(shell pkg-config --cflags hwloc)
HWLOC_LDLIBS    = $(shell pkg-config --libs hwloc)

# Building libmpibind 
VER = 1
MIN = 0
REL = 1
MPIBIND_LIB     = libmpibind.so
MPIBIND_SONAME  = $(MPIBIND_LIB).$(VER)
MPIBIND_FNAME   = $(MPIBIND_SONAME).$(MIN).$(REL)

# Using libmpibind 
MPIBIND_DIR     = $(shell pwd)
MPIBIND_CFLAGS  = -I$(MPIBIND_DIR) $(HWLOC_CFLAGS)
MPIBIND_LDLIBS  = -L$(MPIBIND_DIR) -lmpibind $(HWLOC_LDLIBS)
ifeq ($(UNAME),Linux)
MPIBIND_LDLIBS += -Wl,-rpath=$(MPIBIND_DIR)
endif


PROGS = main

## HWLOC_XML_VERBOSE=1
## HWLOC_XMLFILE=../topo-xml/coral-lassen.xml ./main

all: $(PROGS) $(MPIBIND_SONAME) $(MPIBIND_LIB)

hwloc_tests: hwloc_tests.c
	$(CC) $(BASIC_CFLAGS) $(HWLOC_CFLAGS) $@.c $(HWLOC_LDLIBS) -o $@

dev_tests: dev_tests.c mpibind.h
	$(CC) $(BASIC_CFLAGS) $(HWLOC_CFLAGS) $@.c $(HWLOC_LDLIBS) -o $@

main: main.c $(MPIBIND_LIB)
	$(CC) $@.c $(BASIC_CFLAGS) $(MPIBIND_CFLAGS) -o $@ $(MPIBIND_LDLIBS)

# Todo
#check:
#install:

$(MPIBIND_SONAME): $(MPIBIND_FNAME)
	ln -s -f $< $@

$(MPIBIND_LIB): $(MPIBIND_FNAME)
	ln -s -f $< $@

$(MPIBIND_FNAME): mpibind.o 
ifeq ($(UNAME),Linux)
	$(CC) -shared -Wl,-soname,$(MPIBIND_SONAME) -o $@ $< $(HWLOC_LDLIBS)
else
	$(CC) -shared -o $@ $< $(HWLOC_LDLIBS)
endif 

mpibind.o: mpibind.c mpibind.h mpibind-priv.h
	$(CC) -fPIC $(BASIC_CFLAGS) $(HWLOC_CFLAGS) -c mpibind.c 


clean:
	rm -f $(PROGS) $(MPIBIND_LIB) $(MPIBIND_SONAME) $(MPIBIND_FNAME) *.dSYM *.o *~
