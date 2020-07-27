SOURCE_DIR = ../src

CFLAGS += -g -Wall -I$(SOURCE_DIR) -Werror
LDFLAGS += -L$(SOURCE_DIR) -lmpibind

HWLOC_LIBS = $(shell pkg-config --libs hwloc)
HWLOC_CFLAGS = $(shell pkg-config --cflags hwloc)

SO_NAME = mpibind_flux.so

mpibind.so: mpibind_plugin.c omp_hello.c
	$(CC) $(CFLAGS) $(HWLOC_CFLAGS) -fPIC -shared -o $(SO_NAME) \
		mpibind_plugin.c $(LDFLAGS) $(HWLOC_LIBS)

debug: CFLAGS = -ggdb3 -Wall -I$(SOURCE_DIR)
debug: mpibind.so

clean:
	rm -f $(SO_NAME)
