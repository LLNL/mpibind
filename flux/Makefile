CFLAGS += -g -Wall -Werror -I../src
LDFLAGS += -L../src -lmpibind

HWLOC_LIBS = $(shell pkg-config --libs hwloc)
HWLOC_CFLAGS = $(shell pkg-config --cflags hwloc)

mpibind.so: mpibind.c 
	$(CC) $(CFLAGS) $(HWLOC_CFLAGS) -fPIC -shared -o mpibind.so mpibind.c $(LDFLAGS) $(HWLOC_LIBS)

clean:
	rm -f mpibind.so
