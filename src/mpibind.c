/******************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory 
 ******************************************************/
#include <hwloc.h>
#include <stdio.h>
#include <string.h>
#include "mpibind.h"
#include "mpibind-priv.h"


/* 
 * Briefly show the hwloc topology. 
 */
#if VERBOSE >= 1
static 
void print_topo_brief(hwloc_topology_t topo)
{
  int depth;
  int topo_depth = hwloc_topology_get_depth(topo); 
  
  for (depth = 0; depth < topo_depth; depth++)
    printf("[%d]: %s[%d]\n", depth,
	   hwloc_obj_type_string(hwloc_get_depth_type(topo, depth)), 
	   hwloc_get_nbobjs_by_depth(topo, depth)); 
}
#endif


/* 
 * mpibind relies on Core objects. If the topology
 * doesn't have them, use an appropriate replacement. 
 * Make sure to always use get_core_type and get_core_depth
 * instead of HWLOC_OBJ_CORE and its depth.
 * Todo: In the future, I may need to have similar functions
 * for NUMA domains.  
 */
static
int get_core_depth(hwloc_topology_t topo)
{
  return hwloc_get_type_or_below_depth(topo, HWLOC_OBJ_CORE);
}


/* Commented out to avoid compiler errors. 
   But, I may use this function later. */ 
#if 0
static
hwloc_obj_type_t get_core_type(hwloc_topology_t topo)
{
  return hwloc_get_depth_type(topo, get_core_depth(topo));
}
#endif 


/* 
 * Get the Hardware SMT level. 
 */ 
static
int get_smt_level(hwloc_topology_t topo)
{
  /* If there are no Core objects, assume SMT-1 */ 
  int level = 1; 
  hwloc_obj_t obj = NULL;
  
  if ( (obj = hwloc_get_next_obj_by_depth(topo, get_core_depth(topo),
					  obj)) ) {
    level = (obj->arity == 0) ? 1 : obj->arity; 
    /* Debug */ 
    //printf("SMT level: %d (obj->arity: %d)\n", level, obj->arity); 
  }
  
  return level;  
}


/*
 * Load the hwloc topology. 
 * This function makes sure GPU devices are visible and also 
 * filters out levels that do not add any structure. 
 */ 
static
void topology_load(hwloc_topology_t topology)
{  
  /* Setting HWLOC_XMLFILE in the environment
     is equivalent to hwloc_topology_set_xml */
#if 0
  if (hwloc_topology_set_xml(topology, TOPO_FILE) < 0) {
    int error = errno;
    printf("Couldn’t load topo file %s: %s\n", TOPO_FILE,
  	   strerror(error));
  }
#endif
  
  /* L1Cache does not add significant info (same as Core).  
     Commenting out in favor of a more general filter below */ 
  //hwloc_topology_set_type_filter(topology, HWLOC_OBJ_L1CACHE, 
  //				 HWLOC_TYPE_FILTER_KEEP_NONE);
  
  /* Remove objects that do not add structure. 
     Warning: This function can collapse the Core and PU levels
     into the PU level. Functions that look for the Core level 
     may break or behave differently!  
     Leaving it in for now, because I have my own 'get_core_*' 
     functions rather than using HWLOC_OBJ_CORE directly */ 
  hwloc_topology_set_all_types_filter(topology,
				      HWLOC_TYPE_FILTER_KEEP_STRUCTURE); 
  
  /* Too general, enabling only OS devices below */ 
  //hwloc_topology_set_io_types_filter(topology,
  //				     HWLOC_TYPE_FILTER_KEEP_IMPORTANT);
  
  /* OS devices are filtered by default, enable to see GPUs */ 
  hwloc_topology_set_type_filter(topology, HWLOC_OBJ_OS_DEVICE,
  				 HWLOC_TYPE_FILTER_KEEP_IMPORTANT); 

  /* Detect the topology */ 
  hwloc_topology_load(topology);
}


/* 
 * Distribute workers over domains 
 * Input:
 *   wks: number of workers
 *  doms: number of domains
 * Output: wk_arr of length doms 
 */ 
static
void distrib(int wks, int doms, int *wk_arr) {
  int i, avg, rem;
  
  avg = wks / doms; 
  rem = wks % doms; 
  
  for (i=0; i<doms; i++)
    if (i < rem)
      wk_arr[i] = avg+1;
    else
      wk_arr[i] = avg;
}


#if VERBOSE >= 1
static
int obj_atts_str(hwloc_obj_t obj, char *str, size_t size)
{
  char tmp[SHORT_STR_SIZE];
  int nc=0; 
  
  /* Common attributes */
  hwloc_obj_type_snprintf(tmp, sizeof(tmp), obj, 1);
  nc += snprintf(str+nc, size-nc, "%s: ", tmp);
  nc += snprintf(str+nc, size-nc, "depth=%d ", obj->depth);
  nc += snprintf(str+nc, size-nc, "l_idx=%d ", obj->logical_index);
  //nc += snprintf(str+nc, size-nc, "gp_idx=0x%llx ", obj->gp_index);
  
  /* IO attributes */
  if (hwloc_obj_type_is_io(obj->type)) {
    // obj->attr->type
    // HWLOC_OBJ_OSDEV_COPROC HWLOC_OBJ_OSDEV_OPENFABRICS
    nc += snprintf(str+nc, size-nc, "name=%s ", obj->name);
    nc += snprintf(str+nc, size-nc, "subtype=%s ", obj->subtype);
  } else {
    nc += snprintf(str+nc, size-nc, "os_idx=%d ", obj->os_index);
    hwloc_bitmap_list_snprintf(tmp, sizeof(tmp), obj->cpuset);
    nc += snprintf(str+nc, size-nc, "cpuset=%s ", tmp);
    hwloc_bitmap_list_snprintf(tmp, sizeof(tmp), obj->nodeset);
    nc += snprintf(str+nc, size-nc, "nodeset=%s ", tmp);
    nc += snprintf(str+nc, size-nc, "arity=%d ", obj->arity);
    nc += snprintf(str+nc, size-nc, "amem=%d ", obj->memory_arity);
    nc += snprintf(str+nc, size-nc, "aio=%d ", obj->io_arity);
  }

  return nc; 
}
#endif 


#if VERBOSE >= 1
static
void print_obj(hwloc_obj_t obj) {
  char str[LONG_STR_SIZE];
  obj_atts_str(obj, str, sizeof(str)); 
  printf("%s\n", str);
}
#endif


/* 
 * Print an array on one line starting with 'head'
 */
#if VERBOSE >=1 
static
void print_array(int *arr, int size, char *label)
{
  int i, nc=0; 
  char str[LONG_STR_SIZE];

  for (i=0; i<size; i++)
    nc += snprintf(str+nc, sizeof(str)-nc, "[%d]=%d ", i, arr[i]);

  printf("%s: %s\n", label, str); 
}
#endif 


/*
 * struct hwloc_info_s ∗ infos
 *   char*name
 *   char*value
 * unsigned infos_count
 */
#if VERBOSE >=4 
static
int obj_info_str(hwloc_obj_t obj, char *str, size_t size)
{
  int i, nc=0;
  
  for (i=0; i<obj->infos_count; i++) 
    nc += snprintf(str+nc, size, "Info: %s = %s\n",
		   obj->infos[i].name, obj->infos[i].value);

  return nc; 
} 
#endif

#if VERBOSE >=4
static
void print_obj_info(hwloc_obj_t obj) {
  char str[LONG_STR_SIZE];
  if ( obj_info_str(obj, str, sizeof(str)) )
    printf("%s", str);
}
#endif 


/*
 * Return the device id of a co-processor object, 
 * otherwise return -1
 */ 
static
int get_device_id(hwloc_obj_t obj, int *vendor)
{
  int device_idx=-1;
  const char *str; 
  char nv_str[] = "NVIDIA";
  char amd_str[] = "Advanced Micro Devices"; 
  
  if (strcmp(obj->subtype, "OpenCL") == 0)
    device_idx =
      atoi(hwloc_obj_get_info_by_name(obj, "OpenCLPlatformDeviceIndex"));
  else if (strcmp(obj->subtype, "CUDA") == 0)
    /* Unfortunately, there's no id field for CUDA devices*/
    sscanf(obj->name, "cuda%d", &device_idx);
  else 
    fprintf(stderr, "Error: Could not get device id from %s\n", obj->name); 
  
  if (vendor && device_idx >= 0) {
    str = hwloc_obj_get_info_by_name(obj, "GPUVendor");
#if VERBOSE >=3
    printf("GPUVendor: %s\n", str); 
#endif
    if ( strncmp(str, nv_str, sizeof(nv_str)-1) == 0 )
      *vendor = MPIBIND_GPU_NVIDIA;
    else if ( strncmp(str, amd_str, sizeof(amd_str)-1) == 0 )
      *vendor = MPIBIND_GPU_AMD;
    else
      fprintf(stderr, "Error: Could not get GPUVendor\n"); 
  }
  
  return device_idx; 
}


/*
 * Return the number of NUMA domains with GPUs. 
 * Output:
 *   numas: OS indexes of the numa domains with GPUs.  
 */
static
int numas_wgpus(hwloc_topology_t topo, hwloc_bitmap_t numas)
{
  hwloc_obj_t obj, parent; 
  
  hwloc_bitmap_zero(numas);

  obj = NULL; 
  while ( (obj = hwloc_get_next_osdev(topo, obj)) != NULL )
    if (obj->attr->osdev.type == HWLOC_OBJ_OSDEV_COPROC) {
      parent = hwloc_get_non_io_ancestor_obj(topo, obj);
      hwloc_bitmap_or(numas, numas, parent->nodeset);
      /* Debug / verbose */
#if VERBOSE >= 4
      print_obj(obj);
      print_obj_info(obj);  
      print_obj(parent);
#endif
    }
  
  return hwloc_bitmap_weight(numas);   
}


/*
 * Input: 
 *   root: A normal object (not numa, io, or misc object). 
 * Output: 
 *   gpus: The co-processors reachable from the root. 
 */
static
int get_gpus(hwloc_obj_t root, hwloc_bitmap_t gpus)
{
  // Don't count the GPUs with a counter inside the loop (num),
  // because a single GPU can be shown by hwloc as both
  // CUDA and OpenCL device. In the future, I may consider
  // checking the PCI ID, which should be the same for a
  // CUDA and an OpenCL device if they are the same GPU,
  // e.g., PCI 04:00.0 or 07:00.0
  // Currently, I assume that each device have a unique id
  // as defined by 'get_device_id'
  // int num = 0; 
  hwloc_bitmap_zero(gpus);
  
  if (root->io_arity > 0) {
    hwloc_obj_t obj = root->io_first_child;
    do {
      if (obj->type == HWLOC_OBJ_OS_DEVICE)
	if (obj->attr->osdev.type == HWLOC_OBJ_OSDEV_COPROC) {
#if VERBOSE >= 1
	  print_obj(obj);
#endif
#if VERBOSE >= 4
	  print_obj_info(obj);
#endif
	  /* Form the output GPU set */ 
	  hwloc_bitmap_set(gpus, get_device_id(obj, NULL));
	  //num++; 
	}
    } while((obj = obj->next_sibling) != NULL);
  }

  //return num; 
  return hwloc_bitmap_weight(gpus); 
}


//static
int get_num_gpus(hwloc_topology_t topo, int *gpu_type)
{
  int res; 
  hwloc_obj_t obj = NULL;
  hwloc_bitmap_t gpus = hwloc_bitmap_alloc();

  hwloc_bitmap_zero(gpus); 
  
  while ( (obj = hwloc_get_next_osdev(topo, obj)) != NULL )
    if (obj->attr->osdev.type == HWLOC_OBJ_OSDEV_COPROC) 
      hwloc_bitmap_set(gpus, get_device_id(obj, gpu_type)); 
  
  res = hwloc_bitmap_weight(gpus);
  hwloc_bitmap_free(gpus); 

  return res; 
}


/*
 * Fill buckets with elements
 * Example: Each bucket has a size [0]=3 [1]=3 [2]=2 [3]=2
 * Filled buckets: 
 * [0]=2,4,6 [1]=8,10,12 [2]=14,16 [3]=18,20
 */
static
void fill_in_buckets(int *elems, int nelems,
		     hwloc_bitmap_t *buckets, int nbuckets)
{
  int i, count, bucket_idx, elem_idx; 

  if (nelems >= nbuckets) {
    int nelems_per_bucket[nbuckets];
    
    /* Distribute nelems over nbuckets */ 
    distrib(nelems, nbuckets, nelems_per_bucket); 
    
    count = 0;
    bucket_idx = 0; 
    for (i=0; i<nelems; i++) {
      hwloc_bitmap_set(buckets[bucket_idx], elems[i]);
      if (++count == nelems_per_bucket[bucket_idx]) {
	count = 0;
	bucket_idx++; 
      }
    }
  } else {
    int nbuckets_per_elem[nelems];
    
    /* Distribute nbuckets over nelems */
    distrib(nbuckets, nelems, nbuckets_per_elem);

    count = 0;
    elem_idx = 0; 
    for (i=0; i<nbuckets; i++) {
      hwloc_bitmap_set(buckets[i], elems[elem_idx]);
      if (++count == nbuckets_per_elem[elem_idx]) {
	count = 0;
	elem_idx++; 
      }
    }
  }


#if VERBOSE >=3
  char str[LONG_STR_SIZE];
  for (i=0; i<nbuckets; i++) { 
    hwloc_bitmap_list_snprintf(str, sizeof(str), buckets[i]);
    printf("bucket[%2d]: %s\n", i, str); 
  }
#endif
}


/*
 * Fill buckets with elements, which are given in a bitmap. 
 * Example: elems={2,4,6,8,10,12,14,16,18,20}, nbuckets=4. 
 * Filled buckets: 
 * [0]=2,4,6 [1]=8,10,12 [2]=14,16 [3]=18,20
 * Each bucket is a bitmap. 
 * It also works when there's less elems than nbuckets. 
 * Example: elems={2,4,6}, buckets=5.
 * Filled buckets: 
 * [0]=2 [1]=2 [2]=4 [3]=4 [4]=6
 */
static
void fill_in_buckets_bitmap(hwloc_bitmap_t elems,
			    hwloc_bitmap_t *buckets, int nbuckets)
{
  int i, count, bucket_idx, elem_idx, curr;
  int nelems = hwloc_bitmap_weight(elems); 
  
  if (nelems >= nbuckets) {
    int nelems_per_bucket[nbuckets];
    
    /* Distribute nelems over nbuckets */ 
    distrib(nelems, nbuckets, nelems_per_bucket); 
    
    count = 0;
    bucket_idx = 0;
    hwloc_bitmap_foreach_begin(i, elems) {
      hwloc_bitmap_set(buckets[bucket_idx], i);
      if (++count == nelems_per_bucket[bucket_idx]) {
	count = 0;
	bucket_idx++; 
      }
    } hwloc_bitmap_foreach_end();
    
  } else {
    int nbuckets_per_elem[nelems];
    
    /* Distribute nbuckets over nelems */
    distrib(nbuckets, nelems, nbuckets_per_elem);
    
    count = 0;
    elem_idx = 0;
    curr = hwloc_bitmap_first(elems); 
    for (i=0; i<nbuckets; i++) {
      hwloc_bitmap_set(buckets[i], curr); 
      if (++count == nbuckets_per_elem[elem_idx]) {
	count = 0;
	elem_idx++;
	curr = hwloc_bitmap_next(elems, curr);
      }
    }
  }

  /* Debug */
#if VERBOSE >=3 
  char str[LONG_STR_SIZE];
  for (i=0; i<nbuckets; i++) { 
    hwloc_bitmap_list_snprintf(str, sizeof(str), buckets[i]);
    printf("bucket[%2d]: %s\n", i, str); 
  }
#endif
}


static
void distrib_and_assign_pus(hwloc_bitmap_t *puset, int nobjs,
			    int pus_per_obj,
			    hwloc_bitmap_t *cpus, int ntasks)
{
  int i, obj, task, curr; 
  hwloc_bitmap_t objs;
  hwloc_bitmap_t bucket[ntasks]; 
  
  objs = hwloc_bitmap_alloc();
  hwloc_bitmap_set_range(objs, 0, nobjs-1);

  for (i=0; i<ntasks; i++)
    bucket[i] = hwloc_bitmap_alloc(); 

  fill_in_buckets_bitmap(objs, bucket, ntasks);

  for (task=0; task<ntasks; task++) {
    /* Get the PU set for each object associated with this task */ 
    hwloc_bitmap_foreach_begin(obj, bucket[task]) {
      curr=-1;
      for (i=0; i<pus_per_obj; i++) {
	curr = hwloc_bitmap_next(puset[obj], curr); 
	hwloc_bitmap_set(cpus[task], curr); 
      }
    } hwloc_bitmap_foreach_end();
    
    hwloc_bitmap_free(bucket[task]); 
  }
}


#if 0
struct topo_level {
  char *name;
  int nobjs;
  int depth;
  int pus_per_obj; 
  hwloc_bitmap_t cpuset; 
};

static
void get_level_info(hwloc_topology_t topo,
		    hwloc_cpuset_t cpuset, int depth, 
		    int pus_per_obj, struct topo_level *info)
{
  int i, nobjs, pu, count, nc; 
  hwloc_obj_t obj;
  char *str; 

  info->cpuset = hwloc_bitmap_alloc();
  info->name = malloc(SHORT_STR_SIZE);
  info->nobjs = 0;
  info->depth = depth;
  info->pus_per_obj = pus_per_obj; 
  str = info->name; 
  
  nobjs = hwloc_get_nbobjs_inside_cpuset_by_depth(topo, cpuset, depth);
  
  /* Get the pus for the given depth and cpuset */ 
  for (i=0; i<nobjs; i++) {
    obj = hwloc_get_obj_inside_cpuset_by_depth(topo, cpuset, depth, i);
    
    count = 0; 
    hwloc_bitmap_foreach_begin(pu, obj->cpuset) {
      hwloc_bitmap_set(info->cpuset, pu);
      info->nobjs++; 
      if (++count == pus_per_obj)
	break;
    } hwloc_bitmap_foreach_end(); 
  }
  
  /* Get the name of the matching level */ 
  nc = hwloc_obj_type_snprintf(str, SHORT_STR_SIZE, obj, 1);
  if (depth == hwloc_get_type_depth(topo, HWLOC_OBJ_CORE))
    snprintf(str+nc, SHORT_STR_SIZE-nc, "SMT%d", pus_per_obj); 
}

static
void free_topo_levels(struct topo_level *levels, int size)
{
  int i;

  for (i=0; i<size; i++) {
    free(levels[i].name);
    hwloc_bitmap_free(levels[i].cpuset); 
  }

  free(levels); 
}

/* 
 * While I can walk down the tree to find out what level 
 * is to be used for given ntasks/nthreads, I would have
 * special cases since the topology does not have SMT-2, 
 * SMT-3, etc. levels. 
 * Thus, this function creates my own level structure that 
 * I can use to manage any request uniformly. 
 */
static
struct topo_level* build_mapping_levels(hwloc_topology_t topo,
					hwloc_obj_t root,
					int *size)
{
  int idx, topo_depth, depth, hw_smt, smt, core_depth, nlevels;
  struct topo_level *topo_levels; 

  hw_smt = get_smt_level(topo); 
  topo_depth = hwloc_topology_get_depth(topo);
  core_depth = hwloc_get_type_depth(topo, HWLOC_OBJ_CORE);
  
  nlevels = topo_depth - root->depth;
  if (hw_smt > 2)
    nlevels += hw_smt - 2;
  topo_levels = calloc(nlevels, sizeof(struct topo_level));
  
  /* Walk the tree starting from the root */
  idx = 0; 
  for (depth=root->depth; depth<topo_depth; depth++) {
    get_level_info(topo, root->cpuset, depth, 1, topo_levels+idx); 
    idx++; 

    /* Build SMT levels starting at the Core+1 level */ 
    if (depth == core_depth) 
      for (smt=2; smt<hw_smt; smt++) {
	get_level_info(topo, root->cpuset, depth, smt, topo_levels+idx);
	idx++; 
      }
  }
  
  *size = idx; 
  return topo_levels; 
}
#endif





/*
 * Input:
 *   root: The root object to start from. 
 *   ntasks: number of MPI tasks.
 *   nthreads: the number of threads per task. If zero, 
 *      set the number of threads appropriately. 
 *   usr_smt: map the workers to this SMT level. 
 * Output: 
 *   cpus: array of one cpuset per task. 
 */
static
void cpu_match(hwloc_topology_t topo, hwloc_obj_t root, int ntasks,
	       int *nthreads_ptr, int usr_smt, hwloc_bitmap_t *cpus)
{
  int i, nwks, nobjs, hw_smt, it_smt, pus_per_obj;
  int depth, core_depth;
  hwloc_obj_t obj;
  hwloc_bitmap_t *cpuset;
#if VERBOSE >= 1
  char str[SHORT_STR_SIZE];
#endif
  
  for (i=0; i<ntasks; i++) 
    hwloc_bitmap_zero(cpus[i]);

  /* it_smt holds the intermediate SMT level */ 
  hw_smt = get_smt_level(topo);
  it_smt = (usr_smt > 1 && usr_smt < hw_smt) ? usr_smt : 0; 

#if VERBOSE >= 4
  printf("hw_smt=%d usr_smt=%d, it_smt=%d\n", hw_smt, usr_smt, it_smt);
#endif 

  core_depth = get_core_depth(topo);
  
  /* If need to calculate nthreads, match at Core or usr-SMT level */
  if (*nthreads_ptr <= 0) {
    depth = (usr_smt < hw_smt) ? core_depth :
      hwloc_topology_get_depth(topo) - 1; 
    nobjs = hwloc_get_nbobjs_inside_cpuset_by_depth(topo,
						    root->cpuset,
						    depth);
    if (it_smt)
      nobjs *= it_smt; 
    
    /* Set the nthreads output */ 
    *nthreads_ptr = (nobjs >= ntasks) ? nobjs/ntasks : 1;
    
#if VERBOSE >= 4
    printf("num_threads/task=%d nobjs=%d\n", *nthreads_ptr, nobjs);
    print_obj(root);
#endif 
  }
  nwks = *nthreads_ptr * ntasks;
  
  /* 
   * I need to always match at the Core level
   * if there are sufficient workers or smt is specified. 
   * Then, if more resources are needed add pus per core as necessary. 
   * The previous scheme of matching at smt-k or pu level does not 
   * work as well because tasks may overlap cores--BFS vs DFS.  
   */ 
  
  /* Walk the tree to find a matching level */
  for (depth=root->depth; depth<=core_depth; depth++) { 
    nobjs =
      hwloc_get_nbobjs_inside_cpuset_by_depth(topo,
					      root->cpuset, depth);
    
    /* Usr smt always matches at the Core level */ 
    if (usr_smt && depth != core_depth)
      continue;
    
    if (nobjs >= nwks || depth==core_depth) {
      /* Get the cpu sets of each object in this level */
      cpuset = calloc(nobjs, sizeof(hwloc_bitmap_t));
      for (i=0; i<nobjs; i++) {
	obj = hwloc_get_obj_inside_cpuset_by_depth(topo, root->cpuset,
						   depth, i);
	cpuset[i] = hwloc_bitmap_dup(obj->cpuset);
#if VERBOSE >= 1
	/* Save the object type */ 
	if (i == 0)
	  hwloc_obj_type_snprintf(str, sizeof(str), obj, 1);
#endif
      }
      
      /* Core level or above should have only 1 PU */ 
      pus_per_obj = 1;
      /* Determine SMT level */ 
      if (usr_smt)
	pus_per_obj = usr_smt;
      else if (depth == core_depth)
	for (i=1; i<=hw_smt; i++)
	  if (nobjs*i >= nwks || i==hw_smt) {
	    pus_per_obj = i;
	    break;
	  }

      distrib_and_assign_pus(cpuset, nobjs, pus_per_obj, cpus, ntasks);  

      /* Verbose */
#if VERBOSE >= 1
      printf("Match: %s-%d depth %d nobjs %d npus %d nwks %d\n",
	     str, pus_per_obj, depth, nobjs, nobjs*pus_per_obj, nwks);
#endif
      
      /* Clean up */ 
      for (i=0; i<nobjs; i++)
	hwloc_bitmap_free(cpuset[i]);
      free(cpuset);
      
      break; 
    }
  }
}

 
/* 
 * Distribute the GPUs reachable by root over num tasks. 
 * Input: 
 *   root: An hwloc object to start from. 
 *   ntasks: The number of tasks.  
 * Output: 
 *   gpus_pt: Element i of this array is a bitmap of the GPUs 
 *            assigned to task i. 
 */ 
static
void gpu_match(hwloc_obj_t root, int ntasks,
	       hwloc_bitmap_t *gpus_pt)
{
  hwloc_bitmap_t gpus;
  int i, devid, num_gpus;
  int *elems; 
  
  /* Get the GPUs of this NUMA */
  gpus = hwloc_bitmap_alloc();
  num_gpus = get_gpus(root, gpus);
#if VERBOSE >=2
  printf("Num GPUs for this NUMA domain: %d\n", num_gpus); 
#endif
  
  /* Distribute GPUs among tasks */
  if (num_gpus > 0) {
    /* Get the GPUs in elems[] for fill_in_buckets */
    elems = calloc(num_gpus, sizeof(int)); 
    i = 0; 
    hwloc_bitmap_foreach_begin(devid, gpus) {
      elems[i++] = devid; 
    } hwloc_bitmap_foreach_end();

    fill_in_buckets(elems, num_gpus, gpus_pt, ntasks); 

    free(elems); 
  }
  
  hwloc_bitmap_free(gpus); 
}


/* 
 * And then pass this as a parameter to this function
 * (this is my 'until' parameter from hwloc_distrib)
 * This is the core function that ties everything together! 
 */
static
int distrib_mem_hierarchy(hwloc_topology_t topo,
			  int ntasks, int nthreads,
			  int gpu_optim, int smt, 
			  int *nthreads_pt, 
			  hwloc_bitmap_t *cpus_pt,
			  hwloc_bitmap_t *gpus_pt)
{
  int i, j, num_numas, nt, np, task_offset;
  int *ntasks_per_numa;
  hwloc_obj_t obj;
  hwloc_bitmap_t io_numa_os_ids = NULL; 
  
  /* Distribute tasks over numa domains */
  if (gpu_optim) { 
    io_numa_os_ids = hwloc_bitmap_alloc(); 
    num_numas = numas_wgpus(topo, io_numa_os_ids);
    /* Verbose */
#if VERBOSE >=2
    char str[LONG_STR_SIZE]; 
    hwloc_bitmap_list_snprintf(str, sizeof(str), io_numa_os_ids);
    printf("%d NUMA domains (for GPUs): %s\n", num_numas, str);
#endif
  } else 
    num_numas = hwloc_get_nbobjs_by_depth(topo, HWLOC_TYPE_DEPTH_NUMANODE);

  if (num_numas <= 0) { 
    fprintf(stderr, "Error: No viable NUMA domains\n");
    return 1;
  }
  
  ntasks_per_numa = calloc(num_numas, sizeof(int));
  distrib(ntasks, num_numas, ntasks_per_numa);
  /* Verbose */
#if VERBOSE >=1
  print_array(ntasks_per_numa, num_numas, "ntasks_per_numa");
#endif

  /* For each NUMA, get the CPUs and GPUs per task */  
  i = 0; 
  obj = NULL;
  task_offset = 0;
  while ((obj=hwloc_get_next_obj_by_depth(topo, HWLOC_TYPE_DEPTH_NUMANODE,
					  obj)) != NULL) {
    if (gpu_optim)
      if (!hwloc_bitmap_isset(io_numa_os_ids, obj->os_index))
	continue; 
    
    /* Get the cpuset for each task assigned to this NUMA */
    nt = nthreads;
    np = ntasks_per_numa[i++];

    /* Some NUMA domains may have 0 tasks if there are more 
       NUMAs than tasks */ 
    if (np == 0)
      continue; 
    
    cpu_match(topo, obj->parent, np, &nt, smt, cpus_pt+task_offset);
    
    /* The calculated num threads is the same for all tasks in this NUMA,
       it may be different for other NUMAs */ 
    for (j=0; j<np; j++)
      nthreads_pt[j+task_offset] = nt;
    
    /* Get the gpuset for each task assigned to this NUMA */
    gpu_match(obj->parent, np, gpus_pt+task_offset);
    
    task_offset+=np; 
  }
  
  /* Clean up */
  free(ntasks_per_numa);
  if (gpu_optim)
    hwloc_bitmap_free(io_numa_os_ids);

  return 0; 
}



#if 0
/* 
 * Deprecated in favor of distrib_greedy, which is more general, 
 * but unlike greedy_singleton, distrib_greedy does not employ 
 * smt, threads, or gpu optimizations. 
 * Give a single task the whole node even if it implies given
 * resources in more than one NUMA domain. This is useful for 
 * Python-based programs and other ML workloads that have their
 * own parallelism but use a single process. Without this function
 * mpibind_distrib would map a single task to the resources 
 * associated with a single NUMA domain. 
 */ 
static
void greedy_singleton(hwloc_topology_t topo,
		      int *nthreads, int usr_smt, 
		      hwloc_bitmap_t cpus,
		      hwloc_bitmap_t gpus)
{
  /* Get all the CPUs */ 
  cpu_match(topo, hwloc_get_root_obj(topo), 1, nthreads, usr_smt, &cpus); 
  
  /* Debug */ 
  printf("nthreads=%d\n", *nthreads);
  
  /* Get all the GPUs */ 
  hwloc_obj_t obj = NULL; 
  while ( (obj = hwloc_get_next_osdev(topo, obj)) != NULL )
    if (obj->attr->osdev.type == HWLOC_OBJ_OSDEV_COPROC) 
      hwloc_bitmap_set(gpus, get_device_id(obj, NULL)); 
}
#endif 



/* 
 * For jobs with less tasks than NUMA domains, 
 * assign all of the node resources even though 
 * it will result in having tasks that span more
 * than one domain. This is useful for Python-based programs 
 * and other ML workloads that have their own parallelism 
 * but use a single process. Without this function
 * mpibind_distrib would map a single task to the resources 
 * associated with a single NUMA domain. 
 */ 
static
int distrib_greedy(hwloc_topology_t topo, int ntasks, int *nthreads_pt, 
		   hwloc_bitmap_t *cpus_pt, hwloc_bitmap_t *gpus_pt)
{
  int i, task, num_numas; 
  int *numas_per_task;
  hwloc_obj_t obj;
  hwloc_bitmap_t gpus;

  for (i=0; i<ntasks; i++) {
    hwloc_bitmap_zero(cpus_pt[i]);
    hwloc_bitmap_zero(gpus_pt[i]);
  }
  
  num_numas = hwloc_get_nbobjs_by_depth(topo, HWLOC_TYPE_DEPTH_NUMANODE);
  if (num_numas <= 0) { 
    fprintf(stderr, "Error: No viable NUMA domains\n");
    return 1;
  }
  
  /* I know that this case has less tasks than NUMAs */ 
  numas_per_task = malloc(ntasks * sizeof(int));
  distrib(num_numas, ntasks, numas_per_task);
  /* Verbose */
#if VERBOSE >=1
  print_array(numas_per_task, ntasks, "numas_per_task");
#endif

#if VERBOSE >= 2
  char str1[SHORT_STR_SIZE], str2[SHORT_STR_SIZE]; 
#endif 
  
  i = 0; 
  obj = NULL;
  task = 0;
  gpus = hwloc_bitmap_alloc(); 
  while ((obj=hwloc_get_next_obj_by_depth(topo, HWLOC_TYPE_DEPTH_NUMANODE,
					  obj)) != NULL) {
    /* Get the CPUs */ 
    hwloc_bitmap_or(cpus_pt[task], cpus_pt[task], obj->parent->cpuset);

    /* Get the GPUs */ 
    get_gpus(obj->parent, gpus);
    hwloc_bitmap_or(gpus_pt[task], gpus_pt[task], gpus);

#if VERBOSE >= 2
    hwloc_bitmap_list_snprintf(str1, sizeof(str1), obj->parent->cpuset);
    hwloc_bitmap_list_snprintf(str2, sizeof(str2), gpus); 
    printf("task %d numa %d gpus %s cpus %s\n", task, i, str2, str1);
#endif
    
    if ( numas_per_task[task] == ++i ) {
      i = 0; 
      task++;
    }
  }

  for (i=0; i<ntasks; i++)
    nthreads_pt[i] = hwloc_bitmap_weight(cpus_pt[i]); 
  
  /* Clean up */ 
  hwloc_bitmap_free(gpus); 
  free(numas_per_task); 

  return 0;   
}


/* 
 * The main mapping function. 
 */ 
//static
int mpibind_distrib(hwloc_topology_t topo,
		    int ntasks, int nthreads,
		    int greedy, int gpu_optim, int smt, 
		    int *nthreads_pt, 
		    hwloc_bitmap_t *cpus_pt,
		    hwloc_bitmap_t *gpus_pt)
{
  int rc, num_numas;
  
  num_numas = hwloc_get_nbobjs_by_depth(topo, HWLOC_TYPE_DEPTH_NUMANODE);
  //printf("num_numas=%d\n", num_numas);

#if 0
  if (greedy && ntasks==1) {
    nthreads_pt[0] = nthreads; 
    greedy_singleton(topo, nthreads_pt, smt, cpus_pt[0], gpus_pt[0]);
    return 0; 
  }
#endif  

  if (greedy && ntasks < num_numas) 
    rc = distrib_greedy(topo, ntasks, nthreads_pt, cpus_pt, gpus_pt);
  else 
    rc = distrib_mem_hierarchy(topo, ntasks, nthreads, gpu_optim, smt, 
			       nthreads_pt, cpus_pt, gpus_pt);
  
  return rc; 
}




/*********************************************
 * Public interface (non-static functions)
 *********************************************/

/* 
 * Initialize an mpibind handle. All mpibind functions require
 * this function to be called first. 
 */ 
int mpibind_init(mpibind_t **handle)
{
  mpibind_t *hdl = malloc(sizeof(mpibind_t));
  if (hdl == NULL) {
    *handle = NULL; 
    return 1;
  }
  
  /* Required parameter */
  hdl->ntasks = 0;
  
  /* Set default values for optional parameters */
  hdl->in_nthreads = 0;
  hdl->greedy = 1;
  hdl->gpu_optim = 1;
  hdl->smt = 0;
  hdl->restr_set = NULL;
  hdl->restr_type = MPIBIND_RESTRICT_CPU;
  hdl->topo = NULL; 

  hdl->nvars = 0;
  hdl->names = NULL; 
  hdl->env_vars = NULL; 
  
  *handle = hdl;
  
  return 0; 
}

/* 
 * Release resources associated with the input handle.
 * After this call, no mpibind function should be called. 
 */ 
int mpibind_finalize(mpibind_t *hdl)
{
  int i, v;
  
  if (hdl == NULL)
    return 1;

  for (i=0; i<hdl->ntasks; i++) {
    hwloc_bitmap_free(hdl->cpus[i]);
    hwloc_bitmap_free(hdl->gpus[i]); 
  }
  free(hdl->cpus);
  free(hdl->gpus);
  free(hdl->nthreads);


  for (v=0; v<hdl->nvars; v++) {
#if VERBOSE >= 3
    printf("Releasing %s\n", hdl->env_vars[v].name);
#endif
    free(hdl->names[v]); 
    free(hdl->env_vars[v].name);
    for (i=0; i<hdl->env_vars[v].size; i++)
      free(hdl->env_vars[v].values[i]); 
    free(hdl->env_vars[v].values); 
  }
  if (hdl->names != NULL)
    free(hdl->names); 
  if (hdl->env_vars != NULL)
    free(hdl->env_vars);
  
  free(hdl);
  
  return 0; 
}

/* 
 * The number of processes/tasks in the job.
 */ 
int mpibind_set_ntasks(mpibind_t *handle,
		       int ntasks)
{
  if (handle == NULL)
    return 1;
  
  handle->ntasks = ntasks; 
  
  return 0; 
}

/*
 * The number of threads per process. 
 * If nthreads is not provided, mpibind calculates an 
 * appropriate number of threads for each task 
 * (see mpibind_get_nthreads). 
 */ 
int mpibind_set_nthreads(mpibind_t *handle,
			 int nthreads)
{
  if (handle == NULL)
    return 1;
  
  handle->in_nthreads = nthreads; 
  
  return 0; 
}

/*
 * Valid values are 0 and 1. Default is 1. 
 * If 1 and the number of tasks is less than the number of 
 * NUMA domains, mpibind assigns all of the available hardware 
 * resources to the input tasks. 
 * Otherwise, mpibind assigns no more than one NUMA domain 
 * (and associated resources) per task. 
 */ 
int mpibind_set_greedy(mpibind_t *handle,
		       int greedy)
{
  if (handle == NULL)
    return 1;
  
  handle->greedy = greedy; 
  
  return 0; 
}

/*
 * Valid values are 0 and 1. Default is 1. 
 * If 1, optimize task placement for GPUs. 
 * Otherwise, optimize placement for CPUs. 
 */  
int mpibind_set_gpu_optim(mpibind_t *handle,
			  int gpu_optim)
{
  if (handle == NULL)
    return 1;
  
  handle->gpu_optim = gpu_optim; 
  
  return 0; 
}

/*
 * Map the application workers to this SMT-level.
 * For an n-way SMT architecture, valid values are 1 to n.
 * If not specified, mpibind determines the SMT level 
 * appropriately based on the number of input workers. 
 */    
int mpibind_set_smt(mpibind_t *handle,
		    int smt)
{
  if (handle == NULL)
    return 1;
  
  handle->smt = smt; 
  
  return 0; 
}

/*
 * Restrict the hardware topology to resources
 * associated with the specified hardware ids of type 'restr_type'.
 */
int mpibind_set_restrict_ids(mpibind_t *handle,
			     char *restr_set)
{
  if (handle == NULL)
    return 1;
  
  handle->restr_set = restr_set; 
  
  return 0; 
}

/*
 * Specify the type of resource to use to restrict 
 * the hardware topology: MPIBIND_RESTRICT_CPU or 
 * MPIBIND_RESTRICT_MEM. 
 * The default value is MPIBIND_RESTRICT_CPU and it only 
 * applies if 'restr_set' is given.
 */ 
int mpibind_set_restrict_type(mpibind_t *handle,
			      int restr_type)
{
  if (handle == NULL)
    return 1;
  
  handle->restr_type = restr_type; 
  
  return 0;   
}

/*
 * Pass a loaded topology to mpibind. 
 * mpibind will use this topology to perform the mappings
 * instead of loading its own topology. 
 */
int mpibind_set_topology(mpibind_t *handle,
			 hwloc_topology_t topo)
{
  if (handle == NULL)
    return 1;
  
  handle->topo = topo; 
  
  return 0;   
}

/*
 * Array with 'ntasks' elements. Each entry correspond 
 * to the number of threads to use for the process/task 
 * corresponding to this entry. 
 */ 
int* mpibind_get_nthreads(mpibind_t *handle)
{
  if (handle == NULL)
    return NULL;

  return handle->nthreads; 
}

/*
 * Array with 'ntasks' elements. The physical CPUs to 
 * use for a given process/task.
 */ 
hwloc_bitmap_t* mpibind_get_cpus(mpibind_t *handle)
{
  if (handle == NULL)
    return NULL;

  return handle->cpus; 
}

/*
 * Array with 'ntasks' elements. The GPUs to use for a 
 * given process/task. 
 */
hwloc_bitmap_t* mpibind_get_gpus(mpibind_t *handle)
{
  if (handle == NULL)
    return NULL;

  return handle->gpus; 
}

/*
 * The GPU vendor associated with the GPUs on this 
 * node: MPIBIND_GPU_TYPE_AMD or MPIBIND_GPUT_TYPE_NVIDIA. 
 */
int mpibind_get_gpu_type(mpibind_t *handle)
{
  if (handle == NULL)
    return -1;
  
  return handle->gpu_type; 
}

/*
 * Get the hwloc loaded topology used by mpibind so that 
 * callers can continue to use it even after mpibind has
 * been finalized. This call, however, must be called 
 * before mpibind_finalize. This also means that the caller
 * is responsible for unloading/freeing the topology. 
 */
hwloc_topology_t mpibind_get_topology(mpibind_t *handle)
{
  if (handle == NULL)
    return NULL;
  
  return handle->topo; 
}

/*
 * Get the number of tasks associated with an 
 * mpibind handle.
 */
int mpibind_get_ntasks(mpibind_t *handle)
{
  if (handle == NULL)
    return -1;
  
  return handle->ntasks; 
}

/*
 * Get whether or not an mpibind handle has
 * been set to bind resources greedily.
 */
int mpibind_get_greedy(mpibind_t *handle)
{
  if (handle == NULL)
    return -1;
  
  return handle->greedy; 
}

/*
 * Get whether or not gpu optimization has been
 * specified on an mpibind handle.
 */
int mpibind_get_gpu_optim(mpibind_t *handle)
{
  if (handle == NULL)
    return -1;
  
  return handle->gpu_optim; 
}

/*
 * Get the given smt setting associated with an 
 * mpibind handle.
 */
int mpibind_get_smt(mpibind_t *handle)
{
  if (handle == NULL)
    return -1;
  
  return handle->smt; 
}

/*
 * Get the restrict id set associated with an 
 * mpibind handle.
 */
char* mpibind_get_restrict_ids(mpibind_t *handle)
{
  if (handle == NULL)
    return NULL;

  return handle->restr_set; 
}

/*
 * Get the restrict type associated with an 
 * mpibind handle.
 */
int mpibind_get_restrict_type(mpibind_t *handle)
{
  if (handle == NULL)
    return -1;
  
  return handle->restr_type; 
}

/*
 * Process the input and call the main mapping function. 
 * Input: 
 *   ntasks: number of tasks.  
 *   nthreads: (optional) number of threads per task. 
 *             Default value should be 0, in which case 
 *             mpibind calculates the nthreads per task. 
 *   greedy: (optional) Use the whole node when using a single task. 
 *           Default value should be 1, in which case 
 *           1-task jobs span the whole node.  
 *   gpu_optim: (optional) optimize placement based on GPUs. 
 *              If 0, optimize based on memory and CPUs. 
 *              Default value should be 1 if allocation has GPUs
 *              and 0 otherwise. 
 *   smt: (optional) map app workers to the specified SMT level. 
 *        Default value should be 0, in which case the matching
 *        level is Core, or SMT-k if enough threads are given. 
 *        If smt>0 workers are always mapped to the specified SMT
 *        level regardless of the given number of threads. 
 * Output: 
 *   nthreads, cpus, gpus, and gpu_type. 
 * Returns 0 on success. 
 */
int mpibind(mpibind_t *hdl)
{
  int i, gpu_optim, rc=0;
  unsigned version, major;
  unsigned long flags;
  hwloc_bitmap_t set;
  
  /* hwloc API version 2 required */ 
  version = hwloc_get_api_version();
  major = version >> 16; 
  if (major != 2) {
    fprintf(stderr, "Error: hwloc API version 2 required. "
	    "Current version %d.%d.%d\n",
	    major, (version>>8)&0xff, version&0xff);
    return 1;
  }
  
  /* Input parameters check */
  if (hdl->ntasks <= 0 || hdl->in_nthreads < 0) {
    fprintf(stderr, "Error: ntasks %d or nthreads %d out of range\n",
	    hdl->ntasks, hdl->in_nthreads);
    return 1;
  }

  if (hdl->topo == NULL) { 
    hwloc_topology_init(&hdl->topo);
    topology_load(hdl->topo);
  } else 
    hwloc_topology_check(hdl->topo);

#if VERBOSE >= 1
  print_topo_brief(hdl->topo);
#endif 
  
  if (hdl->smt < 0 || hdl->smt > get_smt_level(hdl->topo)) {
    fprintf(stderr, "Error: SMT parameter %d out of range\n",
	    hdl->smt); 
    return 1;
  }

  /* User asked to restrict the topology */ 
  if (hdl->restr_set) {
    flags = 0; 
    set = hwloc_bitmap_alloc();
    hwloc_bitmap_list_sscanf(set, hdl->restr_set);
    
    if (hdl->restr_type == MPIBIND_RESTRICT_CPU)
      flags = HWLOC_RESTRICT_FLAG_REMOVE_CPULESS;
    else if (hdl->restr_type == MPIBIND_RESTRICT_MEM)
      flags = HWLOC_RESTRICT_FLAG_BYNODESET |
	HWLOC_RESTRICT_FLAG_REMOVE_MEMLESS; 
    
    if ( hwloc_topology_restrict(hdl->topo, set, flags) ) { 
      perror("hwloc_topology_restrict");
      hwloc_bitmap_free(set);
      return errno; 
    }

#if VERBOSE >= 1
    char str[LONG_STR_SIZE];
    hwloc_bitmap_list_snprintf(str, sizeof(str), set);
    printf("Restricted topology: %s with flags %lu\n", str, flags);
#endif
    
    hwloc_bitmap_free(set);
  }

  /* If there's no GPUs, mapping should be CPU-guided.
     Set the type of GPU as well */
  gpu_optim = (get_num_gpus(hdl->topo, &hdl->gpu_type)) ? 1 : 0; 
  gpu_optim &= hdl->gpu_optim;
  
  /* Allocate space to store the resulting mapping */
  hdl->nthreads = calloc(hdl->ntasks, sizeof(int));
  hdl->cpus = calloc(hdl->ntasks, sizeof(hwloc_bitmap_t));
  hdl->gpus = calloc(hdl->ntasks, sizeof(hwloc_bitmap_t)); 
  for (i=0; i<hdl->ntasks; i++) {
    hdl->cpus[i] = hwloc_bitmap_alloc();
    hdl->gpus[i] = hwloc_bitmap_alloc();
  }

#if VERBOSE >= 1
  printf("Input: tasks %d threads %d greedy %d smt %d\n",
	 hdl->ntasks, hdl->in_nthreads, hdl->greedy, hdl->smt); 
  printf("GPUs: number %d optim %d type %d\n",
	 get_num_gpus(hdl->topo, NULL), gpu_optim, hdl->gpu_type);
#endif 

  /* Calculate the mapping. 
     I could pass the mpibind handle, but using explicit 
     parameters for now. */
  rc = mpibind_distrib(hdl->topo, hdl->ntasks, hdl->in_nthreads,
		       hdl->greedy, gpu_optim, hdl->smt, 
		       hdl->nthreads, hdl->cpus, hdl->gpus);
  
  /* Don't clean up, because the caller may need the topology
     to parse the resulting cpu/gpu bitmaps */ 
  //hwloc_topology_destroy(topo);
  
  return rc; 
}

/* 
 * Keep a consistent format for printing the mapping. 
 */ 
#define OUT_FMT "mpibind: task %2d thds %2d gpus %3s cpus %s\n" 

/*
 * Print the mapping for each task. 
 */
void mpibind_print_mapping(mpibind_t *handle)
{
  int i;
  char str1[LONG_STR_SIZE], str2[LONG_STR_SIZE];
  
  for (i=0; i<handle->ntasks; i++) {
    hwloc_bitmap_list_snprintf(str1, sizeof(str1), handle->cpus[i]);
    hwloc_bitmap_list_snprintf(str2, sizeof(str2), handle->gpus[i]);
    printf(OUT_FMT, i, handle->nthreads[i], str2, str1); 
  }
}

/*
 * Print the mapping for a given task. 
 */
void mpibind_print_mapping_task(mpibind_t *handle, int task)
{
  char str1[LONG_STR_SIZE], str2[LONG_STR_SIZE];

  if (task >= 0 && task < handle->ntasks) {
    hwloc_bitmap_list_snprintf(str1, sizeof(str1), handle->cpus[task]);
    hwloc_bitmap_list_snprintf(str2, sizeof(str2), handle->gpus[task]);
    printf(OUT_FMT, task, handle->nthreads[task], str2, str1); 
  }
}

/*
 * Print the mapping to a string. 
 */ 
int mpibind_snprint_mapping(mpibind_t *handle, char *str, size_t size)
{
  int i, nc=0;
  char str1[LONG_STR_SIZE], str2[LONG_STR_SIZE];
  
  for (i=0; i<handle->ntasks; i++) {
    hwloc_bitmap_list_snprintf(str1, sizeof(str1), handle->cpus[i]);
    hwloc_bitmap_list_snprintf(str2, sizeof(str2), handle->gpus[i]);
    nc += snprintf(str+nc, size, OUT_FMT,
                   i, handle->nthreads[i], str2, str1);
  }

  return nc; 
} 

/*
 * Environment variables that need to be exported by the runtime. 
 * CUDA_VISIBLE_DEVICES --comma separated 
 * ROCR_VISIBLE_DEVICES
 * OMP_NUM_THREADS
 * OMP_PLACES --comma separated, each item in curly braces. 
 * OMP_PROC_BIND --spread
 */
int mpibind_set_env_vars(mpibind_t *handle)
{
  int i, v, nc, val, end;
  char *str;
  int nvars = 4;
  char *vars[] = {"OMP_NUM_THREADS",
		  "OMP_PLACES",
		  "OMP_PROC_BIND",
		  "VISIBLE_DEVICES"};

  if (handle == NULL)
    return 1; 

  /* Initialize/allocate env */  
  handle->nvars = nvars;
  handle->env_vars = calloc(nvars, sizeof(mpibind_env_var));

  for (v=0; v<nvars; v++) {    
    /* Fill in env_vars */ 
    handle->env_vars[v].size = handle->ntasks;
    handle->env_vars[v].name = malloc(SHORT_STR_SIZE);
    handle->env_vars[v].values = calloc(handle->ntasks, sizeof(char *)); 
    sprintf(handle->env_vars[v].name, "%s", vars[v]);
    /* Debug */ 
    //printf("Var: %s\n", env->vars[v].name); 
    
    for (i=0; i<handle->ntasks; i++) {
      (handle->env_vars[v].values)[i] = malloc(LONG_STR_SIZE); 
      str = (handle->env_vars[v].values)[i];
      str[0] = '\0';

      if ( strncmp(vars[v], "OMP_NUM_THREADS", 8) == 0 )
	snprintf(str, LONG_STR_SIZE, "%d", handle->nthreads[i]);

      else if ( strncmp(vars[v], "OMP_PLACES", 8) == 0 ) {
	nc = 0;
	hwloc_bitmap_foreach_begin(val, handle->cpus[i]) {
	  nc += snprintf(str+nc,
			 (LONG_STR_SIZE-nc < 0) ? 0 : LONG_STR_SIZE-nc, 
			 "{%d},", val);
	} hwloc_bitmap_foreach_end();
      }
      
      else if ( strncmp(vars[v], "OMP_PROC", 8) == 0 )  
	snprintf(str, LONG_STR_SIZE, "spread");
      
      else if ( strncmp(vars[v], "VISIBLE_DEVICES", 8) == 0 ) {
	if (handle->gpu_type == MPIBIND_GPU_AMD)
	  snprintf(handle->env_vars[v].name, SHORT_STR_SIZE,
		   "ROCR_VISIBLE_DEVICES");
	else if (handle->gpu_type == MPIBIND_GPU_NVIDIA)
	  snprintf(handle->env_vars[v].name, SHORT_STR_SIZE,
		   "CUDA_VISIBLE_DEVICES");
	nc = 0; 
	hwloc_bitmap_foreach_begin(val, handle->gpus[i]) {
	  nc += snprintf(str+nc, LONG_STR_SIZE-nc, "%d,", val); 
	} hwloc_bitmap_foreach_end();
      }
      
      /* Strip the last comma */
      end = strlen(str) - 1; 
      if (str[end] == ',')
	str[end] = '\0'; 
    } 
  }

  /* Store the names of the vars in its own array
     so that callers can retrieve them easily */
  handle->names = calloc(nvars, sizeof(char *)); 
  for (v=0; v<nvars; v++) {
    handle->names[v] = malloc(SHORT_STR_SIZE); 
    sprintf(handle->names[v], "%s", handle->env_vars[v].name);
  }
  
  return 0; 
}


void mpibind_print_env_vars(mpibind_t *handle)
{
  int i, v; 

  for (v=0; v<handle->nvars; v++) {
    printf("%s:\n", handle->env_vars[v].name);
    for (i=0; i<handle->env_vars[v].size; i++) {
      printf("  [%d]: %s\n", i, handle->env_vars[v].values[i]); 
    }
  }
}


char** mpibind_get_env_var_values(mpibind_t *handle,
				  char *name)
{
  int v;
  
  if (handle == NULL)
    return NULL;

  /* Always check that the input name is valid */
  for (v=0; v<handle->nvars; v++)
    if (strcmp(handle->env_vars[v].name, name) == 0)
      return handle->env_vars[v].values; 
  
  return NULL; 
}

char** mpibind_get_env_var_names(mpibind_t *handle, int *count)
{
  if (handle == NULL) {
    *count = 0; 
    return NULL;
  }
  
  *count = handle->nvars;
  return handle->names; 
}


