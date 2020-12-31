/******************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory 
 ******************************************************/

#include <hwloc.h>
#include <hwloc/plugins.h>
#include <stdio.h>
#include <string.h>

#define SHORT_STR_SIZE 32
#define LONG_STR_SIZE 1024

void show_quick_topo(hwloc_topology_t topo)
{
  int depth;
  int topo_depth = hwloc_topology_get_depth(topo); 
  
  for (depth = 0; depth < topo_depth; depth++)
    printf("[%d]: %s[%d]\n", depth,
	   hwloc_obj_type_string(hwloc_get_depth_type(topo, depth)), 
	   hwloc_get_nbobjs_by_depth(topo, depth)); 
}  

void get_cpuset_of_nobjs(hwloc_topology_t topo,
			 int nobjs, hwloc_obj_type_t type, 
			 hwloc_bitmap_t cpuset)
{
  int i;
  hwloc_obj_t obj;
  
  hwloc_bitmap_zero(cpuset); 

  for (i=0; i<nobjs; i++) {
    obj = hwloc_get_obj_by_type(topo, type, i);
    hwloc_bitmap_or(cpuset, cpuset, obj->cpuset); 
  }
}

void test_distrib(hwloc_topology_t topo, int wks)
{
  hwloc_obj_t root = hwloc_get_root_obj(topo);
  int i, n_roots = 1, flags = 0;
  int until = INT_MAX;
  hwloc_bitmap_t set[wks];
  char str[128]; 
  
  for (i=0; i<wks; i++) 
    set[i] = hwloc_bitmap_alloc();
  
  /* The 'until' parameter does not seem to have an effect when 
     its value is greater than the Core depth. In other words, 
     even when having multiple PUs per Core and assigning 'until'
     to PU's depth, the distrib function assigns a full core to 
     a task (as opposed to a single PU per task). 
     Also, if there are more cores than tasks, the last task 
     gets all remaining cores, while the first n-1 tasks get 
     a single core */ 
  hwloc_distrib(topo, &root, n_roots, set, wks, until, flags);
  
  for (i=0; i<wks; i++) {
    hwloc_bitmap_list_snprintf(str, sizeof(str), set[i]);
    printf("[%d]: %s\n", i, str); 
    hwloc_bitmap_free(set[i]);
  }			  
}

int restr_topo_to_n_cores(hwloc_topology_t topo, int ncores)
{
  hwloc_bitmap_t restr = hwloc_bitmap_alloc();
  
  get_cpuset_of_nobjs(topo, ncores, HWLOC_OBJ_CORE, restr); 
  
  /* REMOVE_CPULESS flag is necessary to achieve correct mappings
     with hwloc_distrib when 'until' parameter is less than the 
     maximum depth */ 
  if ( hwloc_topology_restrict(topo, restr,
  			       HWLOC_RESTRICT_FLAG_REMOVE_CPULESS) ) { 
    perror("hwloc_topology_restrict");
    hwloc_bitmap_free(restr);
    return errno; 
  }
  
  hwloc_bitmap_free(restr);
  return 0; 
}

int obj_info_str(hwloc_obj_t obj, char *str, size_t size)
{
  int i, nc=0;
  
  for (i=0; i<obj->infos_count; i++) 
    nc += snprintf(str+nc, size, "Info: %s = %s\n",
		   obj->infos[i].name, obj->infos[i].value);

  return nc; 
} 

/* 
 * Get the PCI Bus ID of an I/O device. 
 */ 
int get_pcibus_id(hwloc_obj_t io_obj, char *buf, size_t size)
{
  hwloc_obj_t obj; 

  if (io_obj->type == HWLOC_OBJ_PCI_DEVICE)
    obj = io_obj; 
  else if (io_obj->parent->type == HWLOC_OBJ_PCI_DEVICE)
    obj = io_obj->parent; 
  else
    return 0; 

  return snprintf(buf, size, "%04x:%02x:%02x.%01x", 
    obj->attr->pcidev.domain, obj->attr->pcidev.bus, 
    obj->attr->pcidev.dev, obj->attr->pcidev.func);
}


/* 
 * Get the Universally Unique ID (UUID) of a GPU device.
 * 
 * The GPU UUID is located in the info attributes of the 
 * associated OSDEV_GPU device (rather than OSDEV_COPROC). 
 * The backend for NVIDIA is NVML and for AMD is RSMI.  
 * NVML GPU OS devices
 *     NVIDIAUUID, NVIDIASerial 
 *     NVML is the NVIDIA Management Library and is included in CUDA.
 * RSMI GPU OS devices
 *     AMDUUID, AMDSerial 
 *     RSMI is the AMD ROCm SMI library
 */
int get_uuid(hwloc_obj_t dev, char *buf, size_t size)
{
  int nc=0; 
  hwloc_obj_t obj=NULL; 

  if (dev->type == HWLOC_OBJ_OS_DEVICE && 
      dev->attr->osdev.type == HWLOC_OBJ_OSDEV_GPU)
        obj = dev;
    
  else if (dev->parent->io_arity > 0) {
    hwloc_obj_t io = dev->parent->io_first_child;
    do {
      if (io->type == HWLOC_OBJ_OS_DEVICE && 
          io->attr->osdev.type == HWLOC_OBJ_OSDEV_GPU)
        obj = io; 
    } while ((io = io->next_sibling) != NULL);
  }

  if (obj) {
    nc = snprintf(buf, size, hwloc_obj_get_info_by_name(obj, "AMDUUID")); 
    if (nc <= 0)
      nc = snprintf(buf, size, hwloc_obj_get_info_by_name(obj, "NVIDIAUUID")); 
  }

  return nc; 
}

/*
 * Print object properties to a string. 
 * I/O objects considered include PCI devices and 
 * certain types of OS devices. 
 * To determine if an object is I/O, one can use 
 * hwloc_obj_type_is_io()
 */
int obj_attrs_str(hwloc_obj_t obj, char *str, size_t size, int verbose)
{
  int nc=0; 
  
  nc += hwloc_obj_type_snprintf(str+nc, size-nc, obj, 1);
  nc += snprintf(str+nc, size-nc, ": depth=%d gp_index=0x%lx ",
           obj->depth, obj->gp_index);
  
  if (hwloc_obj_type_is_normal(obj->type)) {
    nc += snprintf(str+nc, size-nc, "\n  ");
    nc += snprintf(str+nc, size-nc, "os_idx=%d ", obj->os_index);
    nc += snprintf(str+nc, size-nc, "l_idx=%d ", obj->logical_index);
    nc += snprintf(str+nc, size-nc, "cpuset=");
    nc += hwloc_bitmap_list_snprintf(str+nc, size-nc, obj->cpuset);
    nc += snprintf(str+nc, size-nc, " nodeset=");
    nc += hwloc_bitmap_list_snprintf(str+nc, size-nc, obj->nodeset);
    nc += snprintf(str+nc, size-nc, " arity=%d ", obj->arity);
    nc += snprintf(str+nc, size-nc, "amem=%d ", obj->memory_arity);
    nc += snprintf(str+nc, size-nc, "aio=%d ", obj->io_arity);
  }

  if (obj->type == HWLOC_OBJ_OS_DEVICE)
  switch (obj->attr->osdev.type) {
    case HWLOC_OBJ_OSDEV_COPROC :
      nc += snprintf(str+nc, size-nc, "subtype=%s ",
        obj->subtype);
    case HWLOC_OBJ_OSDEV_GPU : 
      nc += snprintf(str+nc, size-nc, "\n  uuid="); 
      nc += get_uuid(obj, str+nc, size-nc);
    case HWLOC_OBJ_OSDEV_OPENFABRICS :
      nc += snprintf(str+nc, size-nc, " busid=");
      nc += get_pcibus_id(obj, str+nc, size-nc);
      nc += snprintf(str+nc, size-nc, " name=%s ",
        obj->name);
      if (verbose > 0) {
        nc += snprintf(str+nc, size-nc, "\n  ");
        /* Get obj->infos in one shot */ 
        nc += hwloc_obj_attr_snprintf(str+nc, size-nc, obj, " ", 1);
      }
    default: 
      break;
  }

  if (obj->type == HWLOC_OBJ_PCI_DEVICE) {
        nc += snprintf(str+nc, size-nc, "\n  ");
        /* Get the obj->infos attributes */ 
        nc += hwloc_obj_attr_snprintf(str+nc, size-nc, obj, " ", 1);
      }

  //hwloc_pci_class_string(obj->attr->pcidev.class_id)

  return nc;
}


void print_obj(hwloc_obj_t obj) {
  char str[LONG_STR_SIZE];
  obj_attrs_str(obj, str, sizeof(str), 0); 
  printf("%s\n", str);
}



/* Todo 12/30/2020 
 * Use the PCI busid (get_pcibus_id) in mpibind to 
 * detect when two devices are the same, e.g., 
 * opencl and cuda. Their obj->parent would be the 
 * same PCI device. 
 * Then use UUID (get_uuid) instead of GPU index 
 * to restrict the topology with VISIBLE_DEVICES. 
 * Make sure to enable PCI devices before loading 
 * the topology. 
 * Replace obj_atts_str() with obj_attr_str().
 * AMD: rocm-smi --showuniqueid --showbus
 */

int main(int argc, char *argv[])
{
  hwloc_topology_t topology;
  hwloc_obj_t obj; 

  hwloc_topology_init(&topology); 
  /* OS devices are filtered by default, enable to see GPUs */
  hwloc_topology_set_type_filter(topology, HWLOC_OBJ_OS_DEVICE,
                                 HWLOC_TYPE_FILTER_KEEP_IMPORTANT);
  /* Include PCI devices to determine whether two GPUs 
     are the same device, i.e., opencl1d1 and cuda1. If so, 
     their parent is the same PCI device */ 
  hwloc_topology_set_type_filter(topology, HWLOC_OBJ_PCI_DEVICE,
                                 HWLOC_TYPE_FILTER_KEEP_IMPORTANT);
  hwloc_topology_load(topology); 
  
  show_quick_topo(topology);

  printf("=====Begin root\n"); 
  print_obj(hwloc_get_root_obj(topology)); 
  printf("=====End root\n");  

  printf("=====Begin PCI devices\n");  
  obj = NULL; 
  while ( (obj = hwloc_get_next_pcidev(topology, obj)) != NULL ) 
    print_obj(obj); 
  printf("=====End PCI devices\n");  

  printf("=====Begin OS devices\n");  
  obj = NULL; 
  while ( (obj = hwloc_get_next_osdev(topology, obj)) != NULL ) {
    print_obj(obj);
  }
  printf("=====End OS devices\n");  

  printf("=====Begin hwloc_restrict\n");
  restr_topo_to_n_cores(topology, 4); 
  show_quick_topo(topology);
  printf("=====End hwloc_restrict\n");
  
  printf("=====Begin hwloc_distrib\n");
  test_distrib(topology, 3);
  printf("=====End hwloc_distrib\n"); 
  
  hwloc_topology_destroy(topology);
  
  return 0; 
}
