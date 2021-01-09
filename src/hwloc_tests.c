/******************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory 
 ******************************************************/

#include <hwloc.h>
#include <hwloc/plugins.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "hwloc_utils.h"




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
  nc += snprintf(str+nc, size-nc, ": depth=%d gp_index=0x%" PRIu64 " ",
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
      nc += gpu_uuid_snprintf(str+nc, size-nc, obj);
    case HWLOC_OBJ_OSDEV_OPENFABRICS :
      nc += snprintf(str+nc, size-nc, " busid=");
      nc += pci_busid_snprintf(str+nc, size-nc, obj);
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


void check_topo_filters(hwloc_topology_t topo)
{
  enum hwloc_type_filter_e f1, f2; 
  hwloc_topology_get_type_filter(topo, 
    HWLOC_OBJ_PCI_DEVICE, &f1);
  hwloc_topology_get_type_filter(topo, 
    HWLOC_OBJ_MISC, &f2);

  if (f1 == HWLOC_TYPE_FILTER_KEEP_IMPORTANT ||
      f1 == HWLOC_TYPE_FILTER_KEEP_ALL)
      printf("PCI devices enabled\n");
  if (f2 == HWLOC_TYPE_FILTER_KEEP_IMPORTANT ||
      f2 == HWLOC_TYPE_FILTER_KEEP_ALL)
      printf("Misc objects enabled\n");
}



int main(int argc, char *argv[])
{
  hwloc_topology_t topology;

  hwloc_topology_init(&topology); 
  /* OS devices are filtered by default, enable to see GPUs */
  hwloc_topology_set_type_filter(topology, HWLOC_OBJ_OS_DEVICE,
                                 HWLOC_TYPE_FILTER_KEEP_IMPORTANT);
  /* Include PCI devices to determine whether two GPUs 
     are the same device, i.e., opencl1d1 and cuda1 */ 
  hwloc_topology_set_type_filter(topology, HWLOC_OBJ_PCI_DEVICE,
                                 HWLOC_TYPE_FILTER_KEEP_IMPORTANT);
  hwloc_topology_load(topology); 
  

  printf("=====Begin brief topology\n");
  print_topo_brief(topology);
  printf("=====End brief topology\n");

  printf("=====Begin I/O topology\n");
  print_topo_io(topology);
  printf("=====End I/O topology\n");

  printf("=====Begin flat list of devices\n");
  print_devices(topology, HWLOC_OBJ_GROUP); 
  print_devices(topology, HWLOC_OBJ_OS_DEVICE); 
  printf("=====End flat list of devices\n");

  printf("=====Begin filter type\n");
  check_topo_filters(topology);
  printf("=====End filter type\n");

#if 0
  /* I haven't been able to use VISIBLE_DEVICES
     within a process to restrict the GPU set */ 
  printf("=====Begin ENV\n"); 
  print_devices(topology, HWLOC_OBJ_OS_DEVICE);

  int rc = putenv("CUDA_VISIBLE_DEVICES=1");
  printf("===CUDA_VISIBLE_DEVICES=1 rc=%d===\n", rc);
  
  hwloc_topology_t topo2; 
  hwloc_topology_init(&topo2);
  hwloc_topology_set_io_types_filter(topo2, 
    HWLOC_TYPE_FILTER_KEEP_IMPORTANT);
  hwloc_topology_load(topo2); 
  print_devices(topo2, HWLOC_OBJ_OS_DEVICE);
  hwloc_topology_destroy(topo2);
  printf("=====End ENV\n");  
#endif

  printf("=====Begin root\n"); 
  print_obj(hwloc_get_root_obj(topology), 0); 
  printf("=====End root\n");  

  printf("=====Begin hwloc_restrict\n");
  restr_topo_to_n_cores(topology, 4); 
  print_topo_brief(topology);
  printf("=====End hwloc_restrict\n");
  
  printf("=====Begin hwloc_distrib\n");
  test_distrib(topology, 3);
  printf("=====End hwloc_distrib\n"); 
  
  hwloc_topology_destroy(topology);
  
  return 0; 
}
