/******************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory 
 ******************************************************/
#include <stdlib.h>
#include <ctype.h>
#include <hwloc.h>
#include "mpibind.h"
#include "mpibind-priv.h"
#include "hwloc_utils.h"


/************************************************
 * Functions needed by the mpibind public 
 * interface implemenation. 
 ************************************************/


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

  PRINT("%s: %s\n", label, str);
}
#endif 




/*
 * Get the PCI bus ID of an input device
 * (save it into input string). 
 * Return a pointer to the associated PCI object. 
 */ 
static 
hwloc_obj_t get_pci_busid(hwloc_obj_t dev, char *busid, int size)
{
  hwloc_obj_t obj; 

  if (dev->type == HWLOC_OBJ_PCI_DEVICE)
    obj = dev; 
  else if (dev->parent->type == HWLOC_OBJ_PCI_DEVICE)
    obj = dev->parent; 
  else 
    return NULL; 

  snprintf(busid, size, "%04x:%02x:%02x.%01x", 
      obj->attr->pcidev.domain, obj->attr->pcidev.bus, 
      obj->attr->pcidev.dev, obj->attr->pcidev.func);

  return obj; 
}


/*
 * Input: 
 *   root: A normal object (not numa, io, or misc object). 
 * Output: 
 *   gpus: The co-processors reachable from the root. 
 */
static
int get_gpus(hwloc_topology_t topo,
	     struct device **devs, int ndevs,
	     hwloc_obj_t root, hwloc_bitmap_t gpus)
{
  int i;
  hwloc_bitmap_zero(gpus);
  
  for (i=0; i<ndevs; i++)
    if (devs[i]->type == DEV_GPU &&
	hwloc_obj_is_in_subtree(topo, devs[i]->ancestor, root))
      hwloc_bitmap_set(gpus, i);
  
  return hwloc_bitmap_weight(gpus);
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
    PRINT("bucket[%2d]: %s\n", i, str);
  }
#endif
}


/*
 * Given a number of objects (nobjs), their cpus (puset), and 
 * a number of tasks, distribute the objects among the tasks. 
 * Each task is assigned the number of cpus indicated by pus_per_obj. 
 * 
 * Updated this function to consider the special case when 
 * nobjs < ntasks. Orignally, two or more tasks would be assigned 
 * the same object, e.g., a core. But, since a core usually has 
 * several PUs, then we can distribute the PUs among the tasks 
 * rather than assigning the same PUs to both. For example, 
 * Originally: 
 *   task 0 -> 0,1 CPUs (core 0)
 *   task 1 -> 0,1 CPUs (core 0)
 *   task 2 -> 2,3 CPUs (core 1)
 * New algorithm: 
 *   task 0 -> 0 
 *   task 1 -> 1
 *   task 2 -> 2,3
 */ 
static
void distrib_and_assign_pus(hwloc_bitmap_t *puset, int nobjs,
			    int pus_per_obj,
			    hwloc_bitmap_t *cpus, int ntasks)
{
  int i, j, curr; 
  hwloc_bitmap_t puset_rev[nobjs]; 

  /* First, create the revised subset of pus for each object
     based on pus_per_obj */ 
  for (j=0; j<nobjs; j++) {
    puset_rev[j] = hwloc_bitmap_alloc();
    curr=-1;
    for (i=0; i<pus_per_obj; i++) {
	    curr = hwloc_bitmap_next(puset[j], curr); 
	    hwloc_bitmap_set(puset_rev[j], curr); 
    }
  }
#if VERBOSE >=2
  char str[100];
  for (i=0; i<nobjs; i++) {
    hwloc_bitmap_list_snprintf(str, sizeof(str), puset_rev[i]);
    PRINT("puset_rev[%d]: %s\n", i, str);
  }  
#endif

  if (nobjs < ntasks) {
    /* Two or more tasks share an object (e.g., core). 
       In this case, distribute the object's PUs over the tasks. */ 
    int ntasks_per_obj[nobjs];
    distrib(ntasks, nobjs, ntasks_per_obj);
    // Print 
#if VERBOSE >= 2
    print_array(ntasks_per_obj, nobjs, "ntasks_per_obj");
#endif 

    // Distribute the pus over tasks 
    j = 0; 
    for (i=0; i<nobjs; i++) { 
      fill_in_buckets_bitmap(puset_rev[i], cpus+j, ntasks_per_obj[i]);
      j += ntasks_per_obj[i]; 
    }
#if VERBOSE >= 2
    for (i=0; i<ntasks; i++) {
      hwloc_bitmap_list_snprintf(str, sizeof(str), cpus[i]);
      PRINT("cpus[%d]: %s\n", i, str);
    }
#endif 
  } else {
    // Original distrib_and_assign_pus 
    hwloc_bitmap_t objs = hwloc_bitmap_alloc();
    hwloc_bitmap_set_range(objs, 0, nobjs-1);

    hwloc_bitmap_t objs_per_task[ntasks]; 
    for (i=0; i<ntasks; i++)
      objs_per_task[i] = hwloc_bitmap_alloc(); 

    /* Distribute the objects among the tasks */ 
    fill_in_buckets_bitmap(objs, objs_per_task, ntasks);
    hwloc_bitmap_free(objs); 

    /* Assign pus_per_obj pus to each task rather than 
       all the pus of each object */ 
    int obj, task;
    for (task=0; task<ntasks; task++) {
      /* Get the PU set for each object associated with this task */ 
      hwloc_bitmap_foreach_begin(obj, objs_per_task[task]) {
        hwloc_bitmap_or(cpus[task], puset_rev[obj], cpus[task]); 
      } hwloc_bitmap_foreach_end();
    
      hwloc_bitmap_free(objs_per_task[task]); 
    }
  }

  // Free up resources 
  for (i=0; i<nobjs; i++)
    hwloc_bitmap_free(puset_rev[i]);   
}



/* 
 * Get the Hardware SMT level. 
 */ 
int get_smt_level(hwloc_topology_t topo)
{
  /* If there are no Core objects, assume SMT-1 */ 
  int level = 1; 
  hwloc_obj_t obj = NULL;
  
  if ( (obj = hwloc_get_next_obj_by_depth(topo, mpibind_get_core_depth(topo),
					  obj)) ) {
    level = (obj->arity == 0) ? 1 : obj->arity; 
    /* Debug */ 
    //printf("SMT level: %d (obj->arity: %d)\n", level, obj->arity); 
  }
  
  return level;  
}


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
  PRINT("hw_smt=%d usr_smt=%d, it_smt=%d\n", hw_smt, usr_smt, it_smt);
#endif 

  core_depth = mpibind_get_core_depth(topo);
  
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
    PRINT("num_threads/task=%d nobjs=%d\n", *nthreads_ptr, nobjs);
    print_obj(root, 0);
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
      //distrib_and_assign_pus_v1(cpuset, nobjs, pus_per_obj, cpus, ntasks);  

      /* Verbose */
#if VERBOSE >= 1
      PRINT("Match: %s-%dPUs depth %d nobjs %d npus %d nwks %d\n",
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
void gpu_match(hwloc_topology_t topo,
	       struct device **devs, int ndevs, 
               hwloc_obj_t root, int ntasks,
	       hwloc_bitmap_t *gpus_pt)
{
  hwloc_bitmap_t gpus;
  int i, devid, num_gpus;
  int *elems; 
  
  /* Get the GPUs of this NUMA */
  gpus = hwloc_bitmap_alloc();
  num_gpus = get_gpus(topo, devs, ndevs, root, gpus);
#if VERBOSE >=2
  PRINT("Num GPUs for this NUMA domain: %d\n", num_gpus);
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
 * Compare the indices of an int array
 * based on the values they point to
 * in descending order
 */
static
int compare_indices_desc(const void *a, const void *b)
{
  /* Input parameter is a pointer to the index
     and thus '**' is needed */
  const int **left  = (const int **)a;
  const int **right = (const int **)b;

  /* Compare the values pointed to by the indexes
     if (left < right) return 1 (positive)
     if (right < left) return -1 (negative)
     else return 0 (equal) */
  return (**left < **right) - (**right < **left);
}


/*
 * Given an array of ints, sort the array in
 * descending order, but instead of sorting
 * the array of values, provide the sorted
 * array of indices.
 */
static
void sort_ints_desc(int *arr, int n, int *indices)
{
  int i;
  int* ptrs[n];

  for (i=0; i<n; i++)
    ptrs[i] = arr + i;

  qsort(ptrs, n, sizeof(int*), compare_indices_desc);

  for (i=0; i<n; i++)
    indices[i] = ptrs[i] - arr;
}


/*
 * Calculate the number of tasks to assign to each
 * NUMA domain. It takes into consideration how many
 * compute units (CPUs or GPUs) each NUMA domain has
 * to make an appropriate assignment. Before this
 * function I was distributing the tasks evenly among
 * NUMA domains.
 */
static
void num_tasks_per_numa(int ntasks, int nnumas, int *cus_per_numa,
			int *ntasks_per_numa)
{
  /* Total number of compute units */
  int i, ncus=0;
  for (i=0; i<nnumas; i++)
    ncus += cus_per_numa[i];

  /* Calculate the num tasks per numa based on the
     fraction of compute units held per NUMA.
     Use floor(ntasks * ncus_per_numa / ncus) first,
     then use the remainder to assign leftover tasks
     (since we are taking the floor) */
  int rem[nnumas];
  int numerator, assigned=0;
  for (i=0; i<nnumas; i++) {
    numerator = ntasks * cus_per_numa[i];
    ntasks_per_numa[i] = numerator / ncus;
    assigned += ntasks_per_numa[i];
    rem[i] = numerator % ncus;
  }

  /* Use the remainder to assign leftover tasks.
     Use the NUMAs with the highests remainders
     to assign an extra task to those NUMAs */
  int indices[nnumas];
  sort_ints_desc(rem, nnumas, indices);

  for (i=0; i<ntasks-assigned; i++)
    ntasks_per_numa[indices[i]] += 1;
}


/*
 * Calculate the number of PUs per NUMA
 * Useful to determine how many tasks per NUMA to assign.
 */
static
void num_pus_per_numa(hwloc_topology_t topo,
		      int nnumas, int *pus_per_numa)
{
  int i = 0;
  hwloc_obj_t obj = NULL;
  while ((obj=hwloc_get_next_obj_by_depth(topo, HWLOC_TYPE_DEPTH_NUMANODE,
					  obj)) != NULL) {
    pus_per_numa[i++] = hwloc_bitmap_weight(obj->cpuset);
  }
}


/*
 * Calculate the number of GPUs per NUMA
 * Useful to determine how many tasks per NUMA to assign.
 */
static
void num_gpus_per_numa(hwloc_topology_t topo,
		       struct device **devs, int ndevs,
		       int nnumas, int *gpus_per_numa)
{
  int i, j;
  for (i=0; i<nnumas; i++)
    gpus_per_numa[i] = 0;

  i = 0;
  hwloc_obj_t obj = NULL;
  while ((obj=hwloc_get_next_obj_by_depth(topo, HWLOC_TYPE_DEPTH_NUMANODE,
					  obj)) != NULL) {
    for (j=0; j<ndevs; j++)
      if (devs[j]->type == DEV_GPU)
	if (hwloc_bitmap_isset(devs[j]->ancestor->nodeset, obj->os_index))
	  gpus_per_numa[i] += 1;
    i++;
  }
}


/* 
 * And then pass this as a parameter to this function
 * (this is my 'until' parameter from hwloc_distrib)
 * This is the core function that ties everything together! 
 */
static
int distrib_mem_hierarchy(hwloc_topology_t topo,
			  struct device **devs, int ndevs,
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

  /* Distribute tasks over numa domains.
     Use the number of compute units within each NUMA
     to balance the tasks accordingly */
#if 1
  num_numas = hwloc_get_nbobjs_by_depth(topo, HWLOC_TYPE_DEPTH_NUMANODE);
  int *cus_per_numa = calloc(num_numas, sizeof(int));
  ntasks_per_numa = calloc(num_numas, sizeof(int));
  
  if (gpu_optim)
    num_gpus_per_numa(topo, devs, ndevs, num_numas, cus_per_numa);
  else
    num_pus_per_numa(topo, num_numas, cus_per_numa);
#if VERBOSE >=1
  print_array(cus_per_numa, num_numas, "ncus_per_numa");
#endif

  num_tasks_per_numa(ntasks, num_numas, cus_per_numa, ntasks_per_numa);
  free(cus_per_numa);
#else
  /* Previous method was to distribute tasks over NUMAs evenly */
  if (gpu_optim) { 
    io_numa_os_ids = hwloc_bitmap_alloc(); 
    //num_numas = numas_wgpus(topo, io_numa_os_ids);
    num_numas = numas_wgpus(devs, ndevs, io_numa_os_ids);
    /* Verbose */
#if VERBOSE >=2
    char str[LONG_STR_SIZE]; 
    hwloc_bitmap_list_snprintf(str, sizeof(str), io_numa_os_ids);
    PRINT("%d NUMA domains (for GPUs): %s\n", num_numas, str);
#endif
  } else 
    num_numas = hwloc_get_nbobjs_by_depth(topo, HWLOC_TYPE_DEPTH_NUMANODE);

  if (num_numas <= 0) { 
    fprintf(stderr, "Error: No viable NUMA domains\n");
    return 1;
  }
  
  ntasks_per_numa = calloc(num_numas, sizeof(int));
  distrib(ntasks, num_numas, ntasks_per_numa);
#endif

#if VERBOSE >=1
  print_array(ntasks_per_numa, num_numas, "ntasks_per_numa");
#endif

  /* For each NUMA, get the CPUs and GPUs per task */  
  i = 0; 
  obj = NULL;
  task_offset = 0;
  while ((obj=hwloc_get_next_obj_by_depth(topo, HWLOC_TYPE_DEPTH_NUMANODE,
					  obj)) != NULL) {
#if 0
    /* Previous method */
    if (gpu_optim)
      if (!hwloc_bitmap_isset(io_numa_os_ids, obj->os_index))
	continue;
#endif
    
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
    gpu_match(topo, devs, ndevs, obj->parent, np, gpus_pt+task_offset);
    
    task_offset+=np; 
  }
  
  /* Clean up */
  free(ntasks_per_numa);
  if (gpu_optim)
    hwloc_bitmap_free(io_numa_os_ids);

  return 0; 
}




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
int distrib_greedy(hwloc_topology_t topo, 
                   struct device **devs, int ndevs,
                   int ntasks, int nthreads, int *nthreads_pt, 
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
    /* The parent object to a NUMA domain may or may not have 
       the GPUs. The GPUs, for example, may be associated with 
       an L3 cache, which is one level down from the object that 
       contains the NUMA domain (Group). get_gpus() now looks 
       for the GPUs down the tree from the given root */
    get_gpus(topo, devs, ndevs, obj->parent, gpus);
    hwloc_bitmap_or(gpus_pt[task], gpus_pt[task], gpus);
    
#if VERBOSE >= 2
    hwloc_bitmap_list_snprintf(str1, sizeof(str1), obj->parent->cpuset);
    hwloc_bitmap_list_snprintf(str2, sizeof(str2), gpus); 
    PRINT("task %d numa %d gpus %s cpus %s\n", task, i, str2, str1);
    print_obj(obj->parent, 1);
#endif
    
    if ( numas_per_task[task] == ++i ) {
      i = 0; 
      task++;
    }
  }

  for (i=0; i<ntasks; i++)
    nthreads_pt[i] =
      (nthreads > 0) ? nthreads : hwloc_bitmap_weight(cpus_pt[i]); 
  
  /* Clean up */ 
  hwloc_bitmap_free(gpus); 
  free(numas_per_task); 

  return 0;   
}


/*
 * Does input object has the given subtype?
 */
static
int obj_has_subtype(hwloc_obj_t obj, char *subtype)
{
  if (obj->subtype && strcmp(obj->subtype, subtype) == 0)
    return 1;
  return 0;
}


/*
 * Get the numeric ID of a device name, e.g.,
 *  rsmi2 -> 2
 *  opencl1d0 -> 10
 *  mlx5_0 -> 50
 */
static
int get_dev_name_id(char *str)
{
  char num[SHORT_STR_SIZE];
  int i, j=0;

  for (i=0; str[i]!='\0'; i++)
    if (isdigit(str[i]))
      num[j++] = str[i];
  num[j] = '\0';

  if (num[0])
    return atoi(num);

  return -1;
}


/* 
 * Input: osdev_obj, an OSDEV object 
 * Output: dev
 * 
 * Fill in information from the OSDEV object into 
 * a device structure. 
 * 
 */
static
void fill_in_device_info(hwloc_obj_t obj, struct device *dev)
{
  hwloc_obj_osdev_type_t type = obj->attr->osdev.type;

  /* Set device name and smi id */
  snprintf(dev->name, SHORT_STR_SIZE, "%s", obj->name);
  dev->smi = get_dev_name_id(dev->name);
  
  /* Set device type, vendor, model, and UUID */
  if (type == HWLOC_OBJ_OSDEV_GPU) {
    dev->type = DEV_GPU;
    /* GPUVendor: AMD, NVIDIA
       Get a single word, e.g. 'NVIDIA' out of 'NVIDIA Corporation' */
    char vendor[SHORT_STR_SIZE];
    sscanf(hwloc_obj_get_info_by_name(obj, "GPUVendor"), "%s", vendor);
    snprintf(dev->vendor, SHORT_STR_SIZE, "%s", vendor);
    snprintf(dev->model, SHORT_STR_SIZE, "%s",
	     hwloc_obj_get_info_by_name(obj, "GPUModel"));
    /* UUID: AMDUUID, NVIDIAUUID */
    char str_uuid[SHORT_STR_SIZE*2];
    snprintf(str_uuid, SHORT_STR_SIZE*2, "%sUUID", dev->vendor);
    snprintf(dev->univ, UUID_LEN, "%s", 
	     hwloc_obj_get_info_by_name(obj, str_uuid));

  } else if (type == HWLOC_OBJ_OSDEV_COPROC) {
    dev->type = DEV_GPU;

    if ( obj_has_subtype(obj, "LevelZero") ) {
      snprintf(dev->vendor, SHORT_STR_SIZE, "%s",
	       hwloc_obj_get_info_by_name(obj, "LevelZeroVendor"));
      snprintf(dev->model, SHORT_STR_SIZE, "%s",
	       hwloc_obj_get_info_by_name(obj, "LevelZeroModel"));
      snprintf(dev->univ, UUID_LEN, "%s",
	       hwloc_obj_get_info_by_name(obj, "LevelZeroUUID"));
    }

  } else if (type == HWLOC_OBJ_OSDEV_OPENFABRICS) {
    dev->type = DEV_NIC;
    snprintf(dev->univ, UUID_LEN, "%s",
	     hwloc_obj_get_info_by_name(obj, "NodeGUID"));

  } else if (type == HWLOC_OBJ_OSDEV_NETWORK) {
    dev->type = DEV_NIC;

    if ( obj_has_subtype(obj, "BXI") )
      snprintf(dev->univ, UUID_LEN, "%s",
	       hwloc_obj_get_info_by_name(obj, "BXIUUID"));
    else
      snprintf(dev->univ, UUID_LEN, "%s",
	       hwloc_obj_get_info_by_name(obj, "Address"));
  }
}


/*
 * Work in progress. Not sure it will be needed.
 *
 * Idea is to identify all the types of GPUs present, e.g.,
 * rsmi, nvml, cuda, opencl, and use one (the *best*) type
 * per device to avoid duplication of devices, e.g., nvml and cuda.
 * For example, if a machine has NVML, OpenCL, and CUDA devices
 * I would use NVML only. If NVML is not present, then CUDA.
 *
 * Currently, I look for NVML and RSMI devices. But, there could
 * be an instance where there's no NVML devices, but there's
 * CUDA devices.
 */
#if 0
#define MAX_GPU_TYPES 16

static
int filter_gpu_types(hwloc_topology_t topo)
{
  int i;
  hwloc_obj_osdev_type_t type;
  char types[MAX_GPU_TYPES][SHORT_STR_SIZE];

  int size = 0;
  hwloc_obj_t obj = NULL;
  while ( (obj = hwloc_get_next_osdev(topo, obj)) != NULL ) {
    type = obj->attr->osdev.type;

#if 0
    /* Debug */
    if (type == HWLOC_OBJ_OSDEV_NETWORK) {
      PRINT("Device: subtype=%s\n", obj->subtype);
      print_obj(obj, 0);
      print_obj_info(obj);
    }
#endif

    if (type == HWLOC_OBJ_OSDEV_COPROC ||
        type == HWLOC_OBJ_OSDEV_GPU) {

      for (i=0; i<size; i++)
	if (obj_has_subtype(obj, types[i]))
	  break;
      if (i == size) {
	/* Got new type, add it */
	if (size >= MAX_GPU_TYPES) {
	  fprintf(stderr, "Warn: GPU-type array maxed out\n");
          return size;
	}
	strcpy(types[size++], obj->subtype);
      }

    }
  }

#if VERBOSE >= 1
  PRINT("GPU types: ");
  for (i=0; i<size; i++) {
    PRINT("%s ", types[i]);
  }
  PRINT("\n");
#endif

  /*
     Then remove unnecesary device types, e.g., cuda/opencl
     when nvml present. Then parse only the selected types
     of devices.
  */

  return size;
}
#endif


/************************************************
 * Non-static functions.
 * Used by mpibind.c
 ************************************************/

/*
 * Input: An hwloc topology.
 * Output: An array of devices.
 *
 * For every unique I/O device, add an entry to the
 * output array with the device's IDs:
 *   PCI Bus ID
 *   Universally Unique ID
 *   SMI ID (RSMI or NVML)
 * This function includes GPUs and NICs
 * and can add other devices as necessary.
 *
 * mpibind uses its own IDs to refer to I/O devices,
 * namely the index of the device ID array.
 * Having small non-negative integers allows using
 * efficient storage: bitmaps.
 * Given an mpibind ID, one can map it to whatever the
 * caller wishes to use, e.g., ID for *_VISIBLE_DEVICES.
 *
 * I rely on RSMI/NVML devices for AMD/NVIDIA GPUs.
 * CUDA/OPENCL IDs are COPROC IDs and relative
 * IDs affected by env vars like CUDA_VISIBLE_DEVICES.
 * NVML and RSMI (GPU IDs), on the other hand, act as absolute
 * IDs--don't change as a result of env vars.
 *
 * A side effect: Setting *_VISIBLE_DEVICES to hide GPUs
 * before calling mpibind may not have an effect, because
 * I look for all devices including RSMI and NVML.
 *
 * I no longer rely on each device having a unique PCI busid:
 * Multiple devices can share one, e.g., partitioned devices.
 * But, every device must have an associated PCI busid.
 */
/*
 * Name   Type        Vendor          Model          Subtype   UUID
 * 
 * rsmi0  GPU         GPUVendor       GPUModel       RSMI      AMDUUID
 * nvml0  GPU         GPUVendor       GPUModel       NVML      NVIDIAUUID
 * ze0    CoProc      LevelZeroVendor LevelZeroModel LevelZero LevelZeroUUID
 * mlx5_0 OpenFabrics ___             ___            ___       NodeGUID
 * hfi1_0 OpenFabrics ___             ___            ___       NodeGUID
 * bxi0   Network     ___             ___            BXI       BXIUUID (hwloc 3)
 * hsi0   Network     ___             ___            Slingshot Address
 */
int discover_devices(hwloc_topology_t topo, 
		     struct device **devs, int size)
{
  int index=0;
  char busid[PCI_BUSID_LEN];
  hwloc_obj_osdev_type_t type;
  hwloc_obj_t pci_obj, obj=NULL;


  while ( (obj = hwloc_get_next_osdev(topo, obj)) != NULL ) {
    type = obj->attr->osdev.type;

    if (type == HWLOC_OBJ_OSDEV_COPROC ||
        type == HWLOC_OBJ_OSDEV_GPU ||
	type == HWLOC_OBJ_OSDEV_NETWORK ||
        type == HWLOC_OBJ_OSDEV_OPENFABRICS) {

      /* Get the PCI bus ID.
	 If a device does not have an associated PCI ID,
	 it will not be recognized/added to the device list */
      pci_obj = get_pci_busid(obj, busid, sizeof(busid)); 
      if (pci_obj == NULL) {
        fprintf(stderr, "Warn: Couldn't get PCI busid of I/O device\n");
        continue;
      }

      /* To add a device, it needs to be:
	 (1) GPU-type device (includes AMD-RSMI and NVIDIA-NVML)
	 (2) OPENFABRICS-type device (includes InfiniBand)
	 (3) COPROC-type: LevelZero device
	 (4) NETWORK-type (includes Slingshot and BXI devices)
	 I exclude CUDA and OpenCL COPROC devices to avoid 
	 duplication with NVML and RSMI devices */
      if ( (type == HWLOC_OBJ_OSDEV_COPROC &&
	    !obj_has_subtype(obj, "LevelZero")) ||
	   (type == HWLOC_OBJ_OSDEV_NETWORK &&
	    !obj_has_subtype(obj, "Slingshot") &&
	    !obj_has_subtype(obj, "BXI")) )
	continue;

      if (index >= size) {
	fprintf(stderr, "Warn: I/O device array maxed out\n");
	return index;
      }

      /* Allocate and initialize the new device */
      devs[index] = malloc(sizeof(struct device));
      devs[index]->univ[0] = '\0';
      devs[index]->vendor[0] = '\0';
      devs[index]->model[0] = '\0';
      devs[index]->type = -1;
      devs[index]->vendor_id = -1;
      devs[index]->smi = -1;

      /* Set vendor and model from the PCI object,
	 mostly for non-GPU devices */
      char vendor[SHORT_STR_SIZE];
      /* Get first word of vendor */
      if (hwloc_obj_get_info_by_name(pci_obj, "PCIVendor")) {
	sscanf(hwloc_obj_get_info_by_name(pci_obj, "PCIVendor"), "%s", vendor);
	snprintf(devs[index]->vendor, SHORT_STR_SIZE, "%s", vendor);
      }
      snprintf(devs[index]->model, SHORT_STR_SIZE, "%s",
	       hwloc_obj_get_info_by_name(pci_obj, "PCIDevice"));

      snprintf(devs[index]->pci, PCI_BUSID_LEN, "%s", busid);
      devs[index]->vendor_id = pci_obj->attr->pcidev.vendor_id;
      devs[index]->ancestor = hwloc_get_non_io_ancestor_obj(topo, obj);

      /* Fill in the rest of the device attributes */
      fill_in_device_info(obj, devs[index]);

      index++;
    }
  }

  return index;
}


/* 
 * The visible GPUs on the system have their 
 * visible devices ID set to a non-negative integer.
 */ 
int get_num_gpus(struct device **devs, int ndevs)
{
  int i, count=0; 

  for (i=0; i<ndevs; i++)
    if (devs[i]->type == DEV_GPU)
      count++; 

  return count; 
}


int get_gpu_vendor_id(struct device **devs, int ndevs)
{
  int i, vendor=-1; 

  for (i=0; i<ndevs; i++)
    if (devs[i]->type == DEV_GPU)
      vendor = devs[i]->vendor_id;

  return vendor;
}


char* get_gpu_vendor(struct device **devs, int ndevs)
{
  int i;
  char* vendor=NULL;

  for (i=0; i<ndevs; i++)
    if (devs[i]->type == DEV_GPU)
      vendor = devs[i]->vendor;

  return vendor; 
}


/* 
 * The main mapping function. 
 */ 
int mpibind_distrib(hwloc_topology_t topo,
		    struct device **devs, int ndevs,
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
    rc = distrib_greedy(topo, devs, ndevs, 
			ntasks, nthreads,
			nthreads_pt, cpus_pt, gpus_pt);
  else 
    rc = distrib_mem_hierarchy(topo, devs, ndevs, 
			       ntasks, nthreads, gpu_optim, smt, 
			       nthreads_pt, cpus_pt, gpus_pt);
  
  return rc; 
}


/*
 * Get a string associated with the specified 
 * ID type for a given device. 
 */
int device_key_snprint(char *buf, size_t size, 
                        struct device *dev, int id_type)
{
  switch (id_type) {
  case MPIBIND_ID_UNIV :
    return snprintf(buf, size, "%s", dev->univ); 
  case MPIBIND_ID_SMI :
    return snprintf(buf, size, "%d", dev->smi);
  case MPIBIND_ID_PCIBUS :
    return snprintf(buf, size, "%s", dev->pci); 
  case MPIBIND_ID_NAME :
    return snprintf(buf, size, "%s", dev->name);
  default: 
    return -1; 
  }
}


/* 
 * Given a PU id, provide the PU set of the core 
 * that contains that PU. 
 */
const hwloc_bitmap_t get_core_cpuset(hwloc_topology_t topo, int pu)
{
  int i, core_depth, ncores;
  hwloc_obj_t core; 
  
  core_depth = mpibind_get_core_depth(topo); 
  ncores = hwloc_get_nbobjs_by_depth(topo, core_depth); 
  
  for (i=0; i<ncores; i++) {
    core = hwloc_get_obj_by_depth(topo, core_depth, i); 
    if ( hwloc_bitmap_isset(core->cpuset, pu) )
      return core->cpuset; 
  }
  
  return NULL; 
}


/*
 * Input:
 *   buf: A set of lines. The last line was partially written.
 *   size: The number of chars in the buffer
 *
 * Either
 *   Remove the partial line or
 *   Overwrite it with [...] if there's space
 */
void terminate_str(char *buf, int size)
{
  /* Look for the beginning of the last line */
  int ch = '\n';
  char *ptr = strrchr(buf, ch) + 1;
  int nchars = buf+size - ptr;
#if DEBUG >=1
  fprintf(OUT_STREAM, "terminate_str: nchars=%d last='%s'\n", nchars, ptr);
#endif
  if (nchars > 6)
    snprintf(ptr, nchars, "[...]\n");
  else
    ptr[0] = '\0';
}


/******************************************************
 * Previous incarnations 
 * Eventually I should move these functions out,
 * perhaps to a separate file. 
 ******************************************************/

/*
 * Replaced this function with num_gpus_per_numa
 * since balancing tasks among numas now takes into
 * consideration the number of compute units per numa.
 *
 * Return the number of NUMA domains with GPUs.
 * Output:
 *   numas: OS indexes of the numa domains with GPUs.
 */
#if 0
static
int numas_wgpus(struct device **devs, int ndevs, hwloc_bitmap_t numas)
{
  int i;

  hwloc_bitmap_zero(numas);

  for (i=0; i<ndevs; i++)
    hwloc_bitmap_or(numas, numas, devs[i]->ancestor->nodeset);

  return hwloc_bitmap_weight(numas);
}
#endif


#if 0
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
      print_obj(obj, 0);
      print_obj_info(obj);  
      print_obj(parent, 0);
#endif
    }
  
  return hwloc_bitmap_weight(numas);   
}
#endif


#if 0
/*
 * Third version of discover_devices()
 *
 * Input: An hwloc topology.
 * Output: An array of devices.
 *
 * For every unique I/O device, add an entry to the
 * output array with the device's different IDs:
 *   PCI Bus ID
 *   Universally Unique ID
 *   *_VISIBLE_DEVICES ID
 *   SMI ID (RSMI or NVML)
 * This function includes GPUs and OpenFabrics NICs
 * and can add other devices as necessary.
 *
 * mpibind uses its own IDs to refer to I/O devices,
 * namely the index of the device ID array.
 * Having small non-negative integers allows using
 * efficient storage: bitmaps.
 * Given an mpibind ID, one can map it to whatever the
 * caller wishes to use, e.g., ID for *_VISIBLE_DEVICES.
 *
 * Notes on the different type of IDs:
 *
 * I no longer rely on cuda<x> or opencl<x>d<y> to
 * detect the device. If rsmi/nvml found, then use these.
 * Note that CUDA/OPENCL are COPROC IDs and are relative
 * IDs affected by env vars like CUDA_VISIBLE_DEVICES.
 * NVML and RSMI (GPU IDs), on the other hand, act as absolute
 * IDs--don't change as a result of env vars.
 *
 * A side effect: Setting *_VISIBLE_DEVICES to hide GPUs
 * before calling mpibind may not have an effect, because
 * I look for all devices including RSMI and NVML.
 *
 * Currently, I rely on the pcibus_id, because it
 * is absolute and it does not rely on having the
 * nvml or rsmi components loaded. These components
 * though are required to get the uuid.
 */
int discover_devices(hwloc_topology_t topo,
		     struct device **devs, int size)
{
  int index, ndevs=0;
  char busid[PCI_BUSID_LEN];
  hwloc_obj_osdev_type_t type;
  hwloc_obj_t pci_obj, obj, current;

  obj = NULL;
  while ( (obj = hwloc_get_next_osdev(topo, obj)) != NULL ) {
    type = obj->attr->osdev.type;
    if (type == HWLOC_OBJ_OSDEV_COPROC ||
        type == HWLOC_OBJ_OSDEV_GPU ||
        type == HWLOC_OBJ_OSDEV_OPENFABRICS) {

      /* Get the PCI bus ID */
      pci_obj = get_pci_busid(obj, busid, sizeof(busid));
      if (pci_obj == NULL) {
        fprintf(stderr, "Warn: Couldn't get PCI busid of I/O device\n");
        continue;
      }

      /* Have we seen this device already?
         opencl0d0 and cuda0 may correspond to the same GPU HW */
      for (index=0; index<ndevs; index++)
        if (strcmp(devs[index]->pci, busid) == 0)
          break;
      /* Add a new device with a unique PCI busid */
      if (index == ndevs) {
        if (ndevs >= size) {
          fprintf(stderr, "Warn: I/O device array maxed out\n");
          return ndevs;
        }

        /* Allocate and initialize the new device */
        devs[index] = malloc(sizeof(struct device));
        devs[index]->univ[0] = '\0';
        devs[index]->smi = -1;
        devs[index]->visdevs = -1;
        devs[index]->type = -1;

        devs[index]->vendor = pci_obj->attr->pcidev.vendor_id;
	devs[index]->ancestor = hwloc_get_non_io_ancestor_obj(topo, obj);

	/* Traverse all objects associated with this PCI device */
	if (pci_obj->io_arity > 0) {
	  current = pci_obj->io_first_child;
	  do {
	    fill_in_device_info(current, devs[index]);
	  } while ((current = current->next_sibling) != NULL);
	}

	/* Make sure VISIBLE_DEVICES id is set,
	   even when no OpenCL device was found (AMD GPUs) */
	if (devs[index]->type == DEV_GPU && devs[index]->visdevs == -1) {
	  if (devs[index]->smi >= 0)
	    devs[index]->visdevs = devs[index]->smi;
	  else
	    fprintf(stderr, "Warn: No ID found for GPU %s\n", busid);
	}

        snprintf(devs[ndevs++]->pci, PCI_BUSID_LEN, "%s", busid);
      }
    }
  }

  return ndevs;
}
#endif


#if 0
/* 
 * Second version of discover devices. 
 * 
 * Input: An hwloc topology. 
 * Output: An array of devices. 
 * 
 * For every unique I/O device, add an entry to the 
 * output array with the device's different IDs: 
 *   PCI Bus ID 
 *   Universally Unique ID 
 *   *_VISIBLE_DEVICES ID 
 *   SMI ID (RSMI or NVML)
 * This function includes GPUs and OpenFabrics NICs 
 * and can add other devices as necessary. 
 * 
 * mpibind uses its own IDs to refer to I/O devices,
 * namely the index of the device ID array. 
 * Having small non-negative integers allows using 
 * efficient storage: bitmaps. 
 * Given an mpibind ID, one can map it to whatever the 
 * caller wishes to use, e.g., ID for *_VISIBLE_DEVICES. 
 *  
 * Notes on the different type of IDs: 
 * I don't like relying on the device ID provided by
 * cuda<x> or opencl<x>d<y>, because these are relative
 * IDs affected by env vars like CUDA_VISIBLE_DEVICES.
 * NVML and RSMI, on the other hand, act as absolute
 * IDs--don't change as a result of env vars. 
 * Currently, I rely on the pcibus_id, because it 
 * is absolute and it does not rely on having the 
 * nvml or rsmi components loaded. These components 
 * though are required to get the uuid. 
 */
int discover_devices(hwloc_topology_t topo, 
                     struct device **devs, int size)
{
  int i, id, index, ndevs=0; 
  char busid[PCI_BUSID_LEN];
  hwloc_obj_osdev_type_t type;
  hwloc_obj_t pci_obj, obj;

  /* First pass: Discover devices that must be added to the 
     input list of devices ('devs'). 
     Do not consider OSDEV_GPU devices in the first passs
     because they do not honor VISIBLE_DEVICES variables--
     If the caller wants to restrict the visible devices, 
     I should honor her request. */ 
  obj = NULL; 
  while ( (obj = hwloc_get_next_osdev(topo, obj)) != NULL ) {
    type = obj->attr->osdev.type; 
    if (type == HWLOC_OBJ_OSDEV_COPROC || 
        type == HWLOC_OBJ_OSDEV_OPENFABRICS) {

      /* Get the PCI bus ID */ 
      pci_obj = get_pci_busid(obj, busid, sizeof(busid)); 
      if (pci_obj == NULL) {
        fprintf(stderr, "Warn: Couldn't get PCI busid of I/O device\n");
        continue; 
      }

      /* Have we seen this device already? 
         opencl0d0 and cuda0 may correspond to the same GPU HW */ 
      for (index=0; index<ndevs; index++) 
        if (strcmp(devs[index]->pci, busid) == 0)
          break; 
      /* Add a new device with a unique PCI busid */ 
      if (index == ndevs) {
        if (ndevs >= size) {
          fprintf(stderr, "Warn: I/O device array maxed out\n"); 
          return ndevs; 
        }

        /* Allocate and initialize the new device */ 
        devs[index] = malloc(sizeof(struct device));
        devs[index]->univ[0] = '\0'; 
        devs[index]->smi = -1;
        devs[index]->visdevs = -1; 
        devs[index]->type = -1; 

        devs[index]->vendor = pci_obj->attr->pcidev.vendor_id;
	devs[index]->ancestor = hwloc_get_non_io_ancestor_obj(topo, obj);
 
        if (type == HWLOC_OBJ_OSDEV_COPROC)
          devs[index]->type = DEV_GPU;
        else if (type == HWLOC_OBJ_OSDEV_OPENFABRICS) {
          devs[index]->type = DEV_NIC;  
          snprintf(devs[index]->univ, UUID_LEN, "%s", 
              hwloc_obj_get_info_by_name(obj, "NodeGUID")); 
        }
        snprintf(devs[index]->name, SHORT_STR_SIZE, "%s", obj->name); 
        if (sscanf(obj->name, "cuda%d", &id) == 1)
          devs[index]->visdevs = id; 
        else if (sscanf(obj->name, "opencl%dd%d", &i, &id) == 2)
          devs[index]->visdevs = id;

        snprintf(devs[ndevs++]->pci, PCI_BUSID_LEN, "%s", busid);
      }
    }
  }

  /* Second pass: Add additional info to existing devices,
     e.g., RSMI/NVML (OSDEV_GPU) devices have the UUID */ 
  obj = NULL; 
  while ( (obj = hwloc_get_next_osdev(topo, obj)) != NULL )
    if (obj->attr->osdev.type == HWLOC_OBJ_OSDEV_GPU) {

      /* Get the PCI bus ID */ 
      get_pci_busid(obj, busid, sizeof(busid)); 

      /* Find the associated device */ 
      for (index=0; index<ndevs; index++) 
        if (strcmp(devs[index]->pci, busid) == 0) {

          /* Add the UUID and System Management ID */ 
          if (sscanf(obj->name, "rsmi%d", &id) == 1) {
            devs[index]->smi = id; 
            snprintf(devs[index]->univ, UUID_LEN, "%s", 
                hwloc_obj_get_info_by_name(obj, "AMDUUID")); 
          } else if (sscanf(obj->name, "nvml%d", &id) == 1) {
            devs[index]->smi = id; 
            snprintf(devs[index]->univ, UUID_LEN, "%s", 
                hwloc_obj_get_info_by_name(obj, "NVIDIAUUID")); 
          }

          break; 
        }
    }

  return ndevs; 
}
#endif


#if 0
/* 
 * First version of discover devices. 
 * It has an issue with ordering of discovery of 
 * devices, for example if OSDEV_GPU is discovered
 * before its associated OSDEV_COPROC
 */ 
static 
int discover_devices(hwloc_topology_t topo, 
                     struct device **devs, int size)
{
  int i, id, index, ndevs=0; 
  char busid[PCI_BUSID_LEN];
  const char *uuid_name[] = { "AMDUUID", "NVIDIAUUID", "NodeGUID" };
  int num_uuids = sizeof(uuid_name) / sizeof(const char *); 
  hwloc_obj_osdev_type_t type;
  hwloc_obj_t pci_obj, obj=NULL;

  while ( (obj = hwloc_get_next_osdev(topo, obj)) != NULL ) {
    type = obj->attr->osdev.type; 
    if (type == HWLOC_OBJ_OSDEV_COPROC || 
        type == HWLOC_OBJ_OSDEV_GPU ||
        type == HWLOC_OBJ_OSDEV_OPENFABRICS) {

      /* Get the PCI bus ID */ 
      if (obj->parent->type == HWLOC_OBJ_PCI_DEVICE) {
        pci_obj = obj->parent; 
        snprintf(busid, sizeof(busid), "%04x:%02x:%02x.%01x", 
        pci_obj->attr->pcidev.domain, pci_obj->attr->pcidev.bus, 
        pci_obj->attr->pcidev.dev, pci_obj->attr->pcidev.func);
      } else {
        fprintf(stderr, "Warn: Couldn't get PCI busid of I/O device\n");
        continue; 
      }
      //printf("Discovering device %s\n", busid);

      /* Have we seen this device already? */ 
      for (index=0; index<ndevs; index++) 
        if (strcmp(devs[index]->pci, busid) == 0)
          break; 
      /* Add new devices to the list--must have
         a unique PCI busid.  
         Don't add RSMI/NVML (OSDEV_GPU) devices, because 
         they do not honor the VISIBLE_DEVICES variables. 
         If the caller wants to restrict the visible 
         devices, I should honor her request. */ 
      if (index == ndevs && type != HWLOC_OBJ_OSDEV_GPU) {
        if (ndevs >= size) {
          fprintf(stderr, "Warn: I/O device array maxed out\n"); 
          return ndevs; 
        }

        /* Allocate and initialize the new device */ 
        devs[index] = malloc(sizeof(struct device));
        devs[index]->univ[0] = '\0'; 
        devs[index]->smi = -1;
        devs[index]->visdevs = -1; 
        devs[index]->type = -1; 

        devs[index]->vendor = pci_obj->attr->pcidev.vendor_id;
        devs[index]->ancestor = 
          hwloc_get_non_io_ancestor_obj(topo, obj)->gp_index; 
        if (type == HWLOC_OBJ_OSDEV_COPROC)
          devs[index]->type = DEV_GPU;
        else if (type == HWLOC_OBJ_OSDEV_OPENFABRICS)
          devs[index]->type = DEV_NIC;  
        snprintf(devs[index]->name, SHORT_STR_SIZE, "%s", obj->name); 
        snprintf(devs[ndevs++]->pci, PCI_BUSID_LEN, "%s", busid);
      }

      /* Add the UUID. For GPUs, UUID comes from OSDEV_GPU */ 
      for (i=0; i<num_uuids; i++)
        if ( hwloc_obj_get_info_by_name(obj, uuid_name[i]) ) {
          /* Save UUID if space has been allocated */ 
          if (devs[index] != NULL) {
            snprintf(devs[index]->univ, UUID_LEN, "%s", 
              hwloc_obj_get_info_by_name(obj, uuid_name[i])); 
          }
          break; 
        }
      
      /* Add the Visible Devices or System Management ID */ 
      if (sscanf(obj->name, "cuda%d", &id) == 1) {
        devs[index]->visdevs = id; 
      } else if (sscanf(obj->name, "opencl%dd%d", &i, &id) == 2) {
        devs[index]->visdevs = id;
      } else if (sscanf(obj->name, "rsmi%d", &id) == 1) {
        if (devs[index] != NULL) 
          devs[index]->smi = id; 
      } else if (sscanf(obj->name, "nvml%d", &id) == 1) {
        if (devs[index] != NULL)
          devs[index]->smi = id; 
      }
    }
  }

  return ndevs;    
}
#endif 


#if 0
/*
 * Commenting out: This function only checks if the co-processor's
 * non-I/O parent is the given root. I replaced this function
 * with a function that indeed checks whether the coprocessors 
 * are reachable from the root.  
 * Input: 
 *   root: A normal object (not numa, io, or misc object). 
 * Output: 
 *   gpus: The co-processors reachable from the root. 
 */
static
int get_gpus(struct device **devs, int ndevs, 
             hwloc_obj_t root, hwloc_bitmap_t gpus)
{
  int i; 
  hwloc_bitmap_zero(gpus);

  for (i=0; i<ndevs; i++)
    if (devs[i]->ancestor == root->gp_index && devs[i]->type == DEV_GPU)
      hwloc_bitmap_set(gpus, i);

  return hwloc_bitmap_weight(gpus);
}
#endif 


#if 0
/* The following functions have been replaced with ones 
   that use the device structure (struct device).
   This change reduces significantly the number of times 
   the hwloc topology is traversed and enables the use
   of different types of IDs such as PCI BUSID */

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
    PRINT("GPUVendor: %s\n", str);
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
 * If the input object is a GPU, add it to the input set. 
 * Function to be passed to tree_walk_io(). 
 */ 
static
void add_gpu_to_set(hwloc_obj_t obj, void *set, int depth)
{
  hwloc_bitmap_t gpus = set; 
  
  if (obj->type == HWLOC_OBJ_OS_DEVICE)
    if (obj->attr->osdev.type == HWLOC_OBJ_OSDEV_COPROC) {
#if VERBOSE >= 1
      print_obj(obj, depth);
#endif
#if VERBOSE >= 4
      print_obj_info(obj);
#endif
      /* Form the output GPU set */
      hwloc_bitmap_set(gpus, get_device_id(obj, NULL));
    }
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
  /* Don't count the GPUs with a counter inside the loop (num),
     because a single GPU can be shown by hwloc as both
     CUDA and OpenCL device. In the future, I may consider
     checking the PCI ID, which should be the same for a
     CUDA and an OpenCL device if they are the same GPU,
     e.g., PCI 04:00.0 or 07:00.0
     Currently, I assume each device has a unique id
     as defined by 'get_device_id' */ 
  hwloc_bitmap_zero(gpus);

  /* Using a deep search instead of a shallow search. 
     There may be other objects, e.g., PCI devices, 
     in between the root object and the GPUs. */ 
#if 0
  if (root->io_arity > 0) {
    hwloc_obj_t obj = root->io_first_child;
    do {
      if (obj->type == HWLOC_OBJ_OS_DEVICE)
        if (obj->attr->osdev.type == HWLOC_OBJ_OSDEV_COPROC) {
#if VERBOSE >= 1
          print_obj(obj, 0);
#endif
#if VERBOSE >= 4
          print_obj_info(obj);
#endif
          /* Form the output GPU set */
          hwloc_bitmap_set(gpus, get_device_id(obj, NULL));
          //num++;
        }
    } while ((obj = obj->next_sibling) != NULL);
  }
#else
  tree_walk_io(add_gpu_to_set, root, gpus, 0);
#endif 

  return hwloc_bitmap_weight(gpus);
}

/*
 * Get number of GPUs.
 */
static
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
#endif 


#if 0
static
int is_osdev_interest(hwloc_obj_t obj)
{
  if (obj->type == HWLOC_OBJ_OS_DEVICE) {
    hwloc_obj_osdev_type_t type = obj->attr->osdev.type; 
    if (type == HWLOC_OBJ_OSDEV_COPROC || 
        type == HWLOC_OBJ_OSDEV_GPU ||
        type == HWLOC_OBJ_OSDEV_OPENFABRICS)
      return 1;
  }
  return 0; 
}
#endif 


#if 0
static
void distrib_and_assign_pus_v1(hwloc_bitmap_t *puset, int nobjs,
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

  /* Distribute the objects among the tasks */ 
  fill_in_buckets_bitmap(objs, bucket, ntasks);

  /* Assign pus_per_obj cpus to each task rather than 
     all the pus of each object */ 
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
#endif 


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


/* Replaced by mpibind_filter_topology */ 
#if 0
/*
 * Load the hwloc topology. 
 * This function makes sure GPU devices are visible and also 
 * filters out levels that do not add any structure. 
 */ 
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

  /* Include PCI devices to determine whether two GPUs 
     are the same device, i.e., opencl1d1 and cuda1 */ 
  hwloc_topology_set_type_filter(topology, HWLOC_OBJ_PCI_DEVICE,
          HWLOC_TYPE_FILTER_KEEP_IMPORTANT);

  /* Detect the topology */ 
  hwloc_topology_load(topology);
}
#endif
