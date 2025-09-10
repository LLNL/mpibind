/******************************************************
 * Edgar A Leon
 * Lawrence Livermore National Laboratory
 ******************************************************/
#ifndef HWLOC_UTILS_H_INCLUDED
#define HWLOC_UTILS_H_INCLUDED

#include <hwloc.h>

#ifdef __cplusplus
extern "C" {
#endif

int pci_busid_snprintf(char *buf, size_t size, hwloc_obj_t io);

int gpu_uuid_snprintf(char *buf, size_t size, hwloc_obj_t dev);

void tree_walk_io(void (*apply)(hwloc_obj_t, void*, int),
                  hwloc_obj_t root, void *args, int depth);

void print_obj_info(hwloc_obj_t obj);

void print_obj(hwloc_obj_t obj, int indent);

void print_devices(hwloc_topology_t topo, hwloc_obj_type_t type);

void print_topo_brief(hwloc_topology_t topo);

void print_topo_io(hwloc_topology_t topo);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // HWLOC_UTILS_H_INCLUDED
