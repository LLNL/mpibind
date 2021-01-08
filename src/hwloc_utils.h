/******************************************************
 * Edgar A Leon
 * Lawrence Livermore National Laboratory 
 ******************************************************/
#ifndef HWLOC_UTILS_H_INCLUDED
#define HWLOC_UTILS_H_INCLUDED


int pci_busid_snprintf(char *buf, size_t size, hwloc_obj_t io); 

int gpu_uuid_snprintf(char *buf, size_t size, hwloc_obj_t dev); 

void print_obj(hwloc_obj_t obj, int indent);

void print_devices(hwloc_topology_t topo, hwloc_obj_type_t type);

void print_topo_brief(hwloc_topology_t topo);

void print_topo_io(hwloc_topology_t topo);


#endif // HWLOC_UTILS_H_INCLUDED