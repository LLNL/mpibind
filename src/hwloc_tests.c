/******************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory 
 ******************************************************/

#include <hwloc.h>
#include <hwloc/plugins.h>
#include <stdio.h>
#include <string.h>


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




int main(int argc, char *argv[])
{
  hwloc_topology_t topology;

  hwloc_topology_init(&topology); 
  hwloc_topology_load(topology); 
  
  show_quick_topo(topology); 
  
  printf("=====Begin hwloc_restrict\n");
  restr_topo_to_n_cores(topology, 4); 
  printf("=====End hwloc_restrict\n");

  show_quick_topo(topology);
  
  printf("=====Begin hwloc_distrib\n");
  test_distrib(topology, 3);
  printf("=====End hwloc_distrib\n"); 
  
  hwloc_topology_destroy(topology);
  
  return 0; 
}
