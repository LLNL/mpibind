/******************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory
 ******************************************************/

#include <hwloc.h>
#include <hwloc/plugins.h>
#include <stdio.h>
#include <string.h>
#include "mpibind.h"

#if 0
enum mp_type {
  CPU, MEM,
  NVIDIA, AMD,
};
typedef enum mp_type en_mp_type;
#endif

struct mp_pair {
  char name[32];
  char value[512];
};
typedef struct mp_pair st_mp_pair;

struct mp_out_env {
  int size;
  st_mp_pair *omp_num;
  st_mp_pair *omp_places;
  st_mp_pair *omp_policy;
  st_mp_pair *gpus;
};
typedef struct mp_out_env st_mp_out_env;

/*
 * Get the Hardware SMT level.
 */
int get_smt_level(hwloc_topology_t topo)
{
  int level = 1;
  hwloc_obj_t obj = NULL;

  if ( (obj = hwloc_get_next_obj_by_type(topo, HWLOC_OBJ_CORE, obj)) ) {
    level = (obj->arity == 0) ? 1 : obj->arity;
    /* Debug */
    //printf("SMT level: %d\n", level);
  }

  return level;
}

/*
 * Create new levels: Core -> SMT2, SMT3 -> PU
 * Save SMT cpus in Core objects, e.g., smt2=8,9 smt3=8,9,10
 * No need to save smt1=8 smt4=8,9,10,11 because they
 * correspond to Core and PU respectively.
 */
void add_smt_info(hwloc_topology_t topo)
{
  int i, smt, level, nc, count;
  hwloc_obj_t obj;
  char val[64], name[8];

  smt = get_smt_level(topo);

  if (smt > 2) {
    /* Annotate the cores with intermediate smt levels */
    obj = NULL;
    while ( (obj = hwloc_get_next_obj_by_type(topo, HWLOC_OBJ_CORE, obj)) ) {
      /* Add levels. No need to add smt-1 (core) or smt-n (PU) */
      for (level=2; level<smt; level++) {
	nc = 0;
	count = 0;
	hwloc_bitmap_foreach_begin(i, obj->cpuset) {
	  nc += snprintf(val+nc, sizeof(val), "%d,", i);
	  if (++count == level)
	    break;
	} hwloc_bitmap_foreach_end();

	snprintf(name, sizeof(name), "smt%d", level);

	hwloc_obj_add_info(obj, name, val);
      }
    }
  }
}

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

  /* Remove objects that do not add structure */
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

  /* Add SMT intermediate levels */
  /* Commenting out since info obtained in place--more efficient */
  //add_smt_info(topology);
}

/*
 * Distribute workers over domains
 * Input:
 *   wks: number of workers
 *  doms: number of domains
 * Output: wk_arr of length doms
 */
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

/* From hwloc handy tree-walk function */
void print_children(hwloc_topology_t topology, hwloc_obj_t obj,
                           int depth)
{
    char type[32], attr[1024];
    unsigned i;

    hwloc_obj_type_snprintf(type, sizeof(type), obj, 0);
    printf("%*s%s", 2*depth, "", type);
    if (obj->os_index != (unsigned) -1)
      printf("#%u", obj->os_index);
    hwloc_obj_attr_snprintf(attr, sizeof(attr), obj, " ", 0);
    if (*attr)
      printf("(%s)", attr);
    printf("\n");
    for (i = 0; i < obj->arity; i++) {
        print_children(topology, obj->children[i], depth + 1);
    }
}

void walk_the_cores(hwloc_topology_t topo)
{
  hwloc_obj_t obj;

  obj = NULL;
  while ( (obj = hwloc_get_next_obj_by_type(topo, HWLOC_OBJ_CORE, obj)) ) {
    print_children(topo, obj, hwloc_get_type_depth(topo, HWLOC_OBJ_CORE));
  }
}

/* How to iterate over a bitmask */
/* hwloc_bitmap_foreach_begin(i, parent->cpuset) { */
/*   //numa_bitmask_setbit(bitmask, i); */
/*   printf("cpuset %d\n", i);  */
/* } hwloc_bitmap_foreach_end(); */

int obj_atts_str(hwloc_obj_t obj, char *str, size_t size)
{
  char tmp[512];
  int nc=0;

  /* Common attributes */
  hwloc_obj_type_snprintf(tmp, sizeof(tmp), obj, 1);
  nc += snprintf(str+nc, size, "%s: ", tmp);
  nc += snprintf(str+nc, size, "depth=%d ", obj->depth);
  nc += snprintf(str+nc, size, "l_idx=%d ", obj->logical_index);
  //nc += snprintf(str+nc, size, "gp_idx=0x%llx ", obj->gp_index);

  /* IO attributes */
  if (hwloc_obj_type_is_io(obj->type)) {
    // obj->attr->type
    // HWLOC_OBJ_OSDEV_COPROC HWLOC_OBJ_OSDEV_OPENFABRICS
    nc += snprintf(str+nc, size, "name=%s ", obj->name);
    nc += snprintf(str+nc, size, "subtype=%s ", obj->subtype);
  } else {
    nc += snprintf(str+nc, size, "os_idx=%d ", obj->os_index);
    hwloc_bitmap_list_snprintf(tmp, sizeof(tmp), obj->cpuset);
    nc += snprintf(str+nc, size, "cpuset=%s ", tmp);
    hwloc_bitmap_list_snprintf(tmp, sizeof(tmp), obj->nodeset);
    nc += snprintf(str+nc, size, "nodeset=%s ", tmp);
    nc += snprintf(str+nc, size, "arity=%d ", obj->arity);
    nc += snprintf(str+nc, size, "amem=%d ", obj->memory_arity);
    nc += snprintf(str+nc, size, "aio=%d ", obj->io_arity);
  }

  return nc;
}

void print_obj(hwloc_obj_t obj) {
  char str[1024];
  obj_atts_str(obj, str, sizeof(str));
  printf("%s\n", str);
}

/*
 * Print an array on one line starting with 'head'
 */
void print_array(int *arr, int size, char *head)
{
  int i, nc=0;
  char str[512];

  for (i=0; i<size; i++)
    nc += snprintf(str+nc, sizeof(str), "[%d]=%d ", i, arr[i]);

  printf("%s: %s\n", head, str);
}

/*
 * struct hwloc_info_s ∗ infos
 *   char*name
 *   char*value
 * unsigned infos_count
 */
int obj_info_str(hwloc_obj_t obj, char *str, size_t size)
{
  int i, nc=0;

  for (i=0; i<obj->infos_count; i++)
    nc += snprintf(str+nc, size, "Info: %s = %s\n",
		   obj->infos[i].name, obj->infos[i].value);

  return nc;
}

void print_obj_info(hwloc_obj_t obj) {
  char str[1024];
  if ( obj_info_str(obj, str, sizeof(str)) )
    printf("%s", str);
}

/*
 * Return the device id of a co-processor object,
 * otherwise return -1
 */
int get_device_id(hwloc_obj_t obj)
{
  int device_idx=-1;

  if (strcmp(obj->subtype, "OpenCL") == 0)
    device_idx =
      atoi(hwloc_obj_get_info_by_name(obj, "OpenCLPlatformDeviceIndex"));
  else if (strcmp(obj->subtype, "CUDA") == 0)
    /* Unfortunately, there's no id field for CUDA devices*/
    sscanf(obj->name, "cuda%d", &device_idx);

  /* debug */
  if (device_idx < 0)
    printf("Error: Could not get device id from %s\n", obj->name);

  return device_idx;
}

/*
 * Return the number of NUMA domains with GPUs.
 * Output:
 *   numas: OS indexes of the numa domains with GPUs.
 */
int numas_wgpus(hwloc_topology_t topo, hwloc_bitmap_t numas)
{
  hwloc_obj_t obj, parent;

  hwloc_bitmap_zero(numas);

  obj = NULL;
  while ( (obj = hwloc_get_next_osdev(topo, obj)) != NULL )
    if (obj->attr->osdev.type == HWLOC_OBJ_OSDEV_COPROC) {
      parent = hwloc_get_non_io_ancestor_obj(topo, obj);
      hwloc_bitmap_or(numas, numas, parent->nodeset);
      print_obj(obj);
      print_obj_info(obj);
      print_obj(parent);
    }

  return hwloc_bitmap_weight(numas);
}

/*
 * Input:
 *   root: A normal object (not numa, io, or misc object).
 * Output:
 *   gpus: The co-processors reachable from the root.
 */
int get_gpus(hwloc_obj_t root, hwloc_bitmap_t gpus)
{
  int num = 0;
  hwloc_bitmap_zero(gpus);

  if (root->io_arity > 0) {
    hwloc_obj_t obj = root->io_first_child;
    do {
      if (obj->type == HWLOC_OBJ_OS_DEVICE)
	if (obj->attr->osdev.type == HWLOC_OBJ_OSDEV_COPROC) {
	  /* verbose or debug */
	  print_obj(obj);
	  //printf("Device Id %d\n", get_device_id(obj));
	  /* Form the output GPU set */
	  hwloc_bitmap_set(gpus, get_device_id(obj));
	  num++;
	}
    } while((obj = obj->next_sibling) != NULL);
  }

  return num;
}

/*
 * Input:
 *     topo: topology
 *     numa: numa domain os index
 *   ntasks: number of MPI tasks
 * nthreads: number of threads
 *           if 0 then calculate num threads
 * Output:
 *    pus: array with a bitmask of cpus per task
 *   gpus: array with a bitmask of gpus per task
 */
void topo_match_old(hwloc_topology_t topo, int numa_os_idx,
		int ntasks, int *nthreads_ptr,
		hwloc_bitmap_t pus, hwloc_bitmap_t gpus)
{
  int nwks, ncores;
  int i, depth, topo_depth, level_size;
  hwloc_obj_t obj, numa_obj, parent;
  char str[128];

  hwloc_bitmap_zero(pus);

  numa_obj = hwloc_get_numanode_obj_by_os_index(topo, numa_os_idx);
  parent = numa_obj->parent;
  printf("Parent of NUMA:\n");
  print_obj(parent);

  /* If need to compute nthreads, match at core level */
  if (*nthreads_ptr <= 0) {
    ncores = hwloc_get_nbobjs_inside_cpuset_by_type(topo, parent->cpuset,
						    HWLOC_OBJ_CORE);
    /* Set the nthreads output */
    *nthreads_ptr = (ncores >= ntasks) ? ncores / ntasks : 1;
  }
  nwks = *nthreads_ptr * ntasks;

  /* Walk the tree to find a level that matches */
  topo_depth = hwloc_topology_get_depth(topo);

  for (depth=parent->depth; depth<topo_depth; depth++) {
    level_size =
      hwloc_get_nbobjs_inside_cpuset_by_depth(topo,
					      parent->cpuset, depth);
    if (level_size >= nwks || depth==topo_depth-1) {
      for (i=0; i<level_size; i++) {
	obj = hwloc_get_obj_inside_cpuset_by_depth(topo, parent->cpuset,
						   depth, i);
	/* Verbose or debug */
	print_obj(obj);
	/* Form the output CPU set */
	hwloc_bitmap_set(pus, hwloc_bitmap_first(obj->cpuset));
      }
      hwloc_obj_type_snprintf(str, sizeof(str), obj, 1);
      break;
    }
  }
  /* Verbose */
  printf("Match: %s depth=%d nobjs=%d\n", str, depth, level_size);

  /* Calculate the GPUs */
  get_gpus(parent, gpus);
}

/*
 * Fill buckets with elements
 * Example: Each bucket has a size [0]=3 [1]=3 [2]=2 [3]=2
 * Filled buckets:
 * [0]=2,4,6 [1]=8,10,12 [2]=14,16 [3]=18,20
 */
void fill_buckets()
{
  int i, n=10, bsize=4, count, b_idx;
  int elems[] = {2, 4, 6, 8, 10, 12, 14, 16, 18, 20};
  int bucket[] = {3, 3, 2, 2};
  hwloc_bitmap_t out[bsize];
  char str[32];

  for (i=0; i<bsize; i++)
    out[i] = hwloc_bitmap_alloc();

  count = 0;
  b_idx = 0;

  for (i=0; i<n; i++) {
    hwloc_bitmap_set(out[b_idx], elems[i]);
    if (++count == bucket[b_idx]) {
      count = 0;
      b_idx++;
    }
  }

  for (i=0; i<bsize; i++) {
    hwloc_bitmap_list_snprintf(str, sizeof(str), out[i]);
    printf("%d: %s\n", i, str);
  }

  for (i=0; i<bsize; i++)
    hwloc_bitmap_free(out[i]);
}

/*
 * Fill buckets with elements
 * Example: Each bucket has a size [0]=3 [1]=3 [2]=2 [3]=2
 * Filled buckets:
 * [0]=2,4,6 [1]=8,10,12 [2]=14,16 [3]=18,20
 */
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

  /* Debug */
  char str[128];
  for (i=0; i<nbuckets; i++) {
    hwloc_bitmap_list_snprintf(str, sizeof(str), buckets[i]);
    printf("bucket[%2d]: %s\n", i, str);
  }
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
  char str[128];
  for (i=0; i<nbuckets; i++) {
    hwloc_bitmap_list_snprintf(str, sizeof(str), buckets[i]);
    printf("bucket[%2d]: %s\n", i, str);
  }
}

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

void test_buckets()
{
  int elems[] = {2, 4, 6, 8, 10, 12, 14, 16, 18, 20};
  int nelems = 10;
  int nbuckets = 4;
  hwloc_bitmap_t buckets[nbuckets];
  int i;

  for (i=0; i<nbuckets; i++)
    buckets[i] = hwloc_bitmap_alloc();

  fill_in_buckets(elems, nelems, buckets, nbuckets);

  for (i=0; i<nbuckets; i++)
    hwloc_bitmap_free(buckets[i]);
}

void get_pus_and_distribute(hwloc_topology_t topo,
			    hwloc_cpuset_t cpuset, int depth,
			    int pus_per_obj,
			    int ntasks, hwloc_bitmap_t *cpus)
{
  int i, nobjs, pu, npus, count;
  hwloc_obj_t obj;
  char str[64];

  nobjs = hwloc_get_nbobjs_inside_cpuset_by_depth(topo, cpuset, depth);
  npus = nobjs * pus_per_obj;

  /* elems stores the pus */
  int *elems = calloc(npus, sizeof(int));

  /* Get the pus for the given depth and cpuset */
  for (i=0; i<nobjs; i++) {
    obj = hwloc_get_obj_inside_cpuset_by_depth(topo, cpuset, depth, i);

    count = 0;
    hwloc_bitmap_foreach_begin(pu, obj->cpuset) {
      elems[i*pus_per_obj + count] = pu;
      if (++count == pus_per_obj)
	break;
    } hwloc_bitmap_foreach_end();
  }
  /* Get the name of the matching level */
  hwloc_obj_type_snprintf(str, sizeof(str), obj, 1);

  /* Distribute the pus over the ntasks and store in 'cpus' */
  fill_in_buckets(elems, npus, cpus, ntasks);

  free(elems);

  /* Verbose */
  printf("Match: %s-%d depth=%d nobjs=%d\n", str, pus_per_obj, depth, npus);
}

struct topo_level {
  char *name;
  int nobjs;
  int depth;
  int pus_per_obj;
  hwloc_bitmap_t cpuset;
};

void get_level_info(hwloc_topology_t topo,
		    hwloc_cpuset_t cpuset, int depth,
		    int pus_per_obj, struct topo_level *info)
{
  int i, nobjs, pu, count, nc;
  hwloc_obj_t obj;
  char *str;

  info->cpuset = hwloc_bitmap_alloc();
  info->name = malloc(64);
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
  nc = hwloc_obj_type_snprintf(str, sizeof(str), obj, 1);
  if (depth == hwloc_get_type_depth(topo, HWLOC_OBJ_CORE))
    snprintf(str+nc, sizeof(str), "SMT%d", pus_per_obj);
}

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

/* Todo: If necessary, I could create an mpibind_handle to
   pass to functions instead of a topology. This handle
   would have built in structures (in addition to the topology)
   I need instead of always passing a large number of arguments
   to each function */

/*
 * mpibind relies on Core objects. If the topology
 * doesn't have them, use an appropriate replacement.
 * Make sure to always use get_core_type and get_core_depth
 * instead of HWLOC_OBJ_CORE and its depth.
 */
int get_core_depth(hwloc_topology_t topo)
{
  return hwloc_get_type_or_below_depth(topo, HWLOC_OBJ_CORE);
}

hwloc_obj_type_t get_core_type(hwloc_topology_t topo)
{
  return hwloc_get_depth_type(topo, get_core_depth(topo));
}

void print_cores(hwloc_topology_t topo)
{
  char str[128];
  hwloc_obj_t obj = NULL;

  while ( (obj=hwloc_get_next_obj_by_type(topo,
					  get_core_type(topo),
					  obj)) ) {
    hwloc_bitmap_list_snprintf(str, sizeof(str), obj->cpuset);
    printf("Core %d: %s\n", obj->os_index, str);
    print_obj_info(obj);
  }
}

/*
 * Next version may take default matching level as a parameter,
 * e.g., core, smt2, or pu.
 * HWLOC_OBJ_CORE, HWLOC_OBJ_PU
 */
void cpu_match_v1(hwloc_topology_t topo, hwloc_obj_t root,
		  int ntasks, int *nthreads_ptr,
		  hwloc_obj_type_t def_level,
		  hwloc_bitmap_t cpus)
{
  int nwks, nobjs, i, depth, topo_depth, level_size;
  hwloc_obj_t obj;
  char str[128];

  hwloc_bitmap_zero(cpus);

  /* If need to compute nthreads, match at core level */
  if (*nthreads_ptr <= 0) {
    nobjs = hwloc_get_nbobjs_inside_cpuset_by_type(topo, root->cpuset,
						    def_level);
    /* Set the nthreads output */
    *nthreads_ptr = (nobjs >= ntasks) ? nobjs / ntasks : 1;
  }
  nwks = *nthreads_ptr * ntasks;

  /* Walk the tree to find a level that matches */
  topo_depth = hwloc_topology_get_depth(topo);

  for (depth=root->depth; depth<topo_depth; depth++) {
    level_size =
      hwloc_get_nbobjs_inside_cpuset_by_depth(topo,
					      root->cpuset, depth);
    if (level_size >= nwks || depth==topo_depth-1) {
      for (i=0; i<level_size; i++) {
	obj = hwloc_get_obj_inside_cpuset_by_depth(topo, root->cpuset,
						   depth, i);
	/* Verbose or debug */
	print_obj(obj);
	/* Form the output CPU set */
	hwloc_bitmap_set(cpus, hwloc_bitmap_first(obj->cpuset));
      }
      hwloc_obj_type_snprintf(str, sizeof(str), obj, 1);
      break;
    }
  }
  /* Verbose */
  printf("Match: %s depth=%d nobjs=%d\n", str, depth, level_size);
}

/*
 * Input:
 *   root: The root object to start from.
 *   ntasks: number of MPI tasks.
 *   nthreads: the number of threads per task. If zero,
 *      set the number of threads to match 'def_level.'
 *   def_level: if nthreads=0 (no user specified), match the
 *      tasks to this level, e.g., HWLOC_OBJ_CORE, HWLOC_OBJ_PU.
 * Output:
 *   cpus: array of one cpuset per task.
 */
void cpu_match_v2(hwloc_topology_t topo, hwloc_obj_t root, int ntasks,
		  int *nthreads_ptr, hwloc_obj_type_t def_level,
		  hwloc_bitmap_t *cpus)
{
  int nwks, nobjs, i, depth, topo_depth, level_size;
  hwloc_obj_t obj;
  char str[128];

  for (i=0; i<ntasks; i++)
    hwloc_bitmap_zero(cpus[i]);

  /* If need to compute nthreads, match at core level */
  if (*nthreads_ptr <= 0) {
    nobjs = hwloc_get_nbobjs_inside_cpuset_by_type(topo, root->cpuset,
						    def_level);
    /* Set the nthreads output */
    *nthreads_ptr = (nobjs >= ntasks) ? nobjs / ntasks : 1;
    /* Debug */
    //printf("num_threads=%d\n", *nthreads_ptr);
  }
  nwks = *nthreads_ptr * ntasks;

  /* Walk the tree to find a level that matches */
  topo_depth = hwloc_topology_get_depth(topo);

  for (depth=root->depth; depth<topo_depth; depth++) {
    level_size =
      hwloc_get_nbobjs_inside_cpuset_by_depth(topo,
					      root->cpuset, depth);

    if (level_size >= nwks || depth==topo_depth-1) {

      /* Store level cpus in elems[] for fill_in_buckets */
      int *elems = calloc(level_size, sizeof(int));
      for (i=0; i<level_size; i++) {
	obj = hwloc_get_obj_inside_cpuset_by_depth(topo, root->cpuset,
						   depth, i);
	elems[i] = hwloc_bitmap_first(obj->cpuset);
      }
      /* Get the name of the matching level */
      hwloc_obj_type_snprintf(str, sizeof(str), obj, 1);

      fill_in_buckets(elems, level_size, cpus, ntasks);

      free(elems);
      break;
#if 0
      int task_idx, count;
      int ncpus_per_task[ntasks];
      /* Distribute num. cpus over num. tasks */
      distrib(level_size, ntasks, ncpus_per_task);
      print_array(ncpus_per_task, ntasks, "ncpus_per_task");

      count = 0;
      task_idx = 0;
      for (i=0; i<level_size; i++) {
	obj = hwloc_get_obj_inside_cpuset_by_depth(topo, root->cpuset,
						   depth, i);
	/* Verbose or debug */
	//print_obj(obj);
	/* Form the output CPU set */
	hwloc_bitmap_set(cpus[task_idx], hwloc_bitmap_first(obj->cpuset));
	if (++count == ncpus_per_task[task_idx]) {
	  count = 0;
	  task_idx++;
	}
      }
#endif
    }

  }
  /* Verbose */
  printf("Match: %s depth=%d nobjs=%d\n", str, depth, level_size);
}

/*
 * Input:
 *   root: The root object to start from.
 *   ntasks: number of MPI tasks.
 *   nthreads: the number of threads per task. If zero,
 *      set the number of threads to match 'def_level.'
 *   def_level: if nthreads=0 (no user specified), match the
 *      tasks to this level, e.g., HWLOC_OBJ_CORE, HWLOC_OBJ_PU.
 * Output:
 *   cpus: array of one cpuset per task.
 */
void cpu_match_smt_v3(hwloc_topology_t topo, hwloc_obj_t root, int ntasks,
		      int *nthreads_ptr, int usr_smt, hwloc_bitmap_t *cpus)
{
  int i, nwks, nobjs, depth, topo_depth, level_size;
  int hw_smt, it_smt;
  hwloc_obj_type_t level_type;

  for (i=0; i<ntasks; i++)
    hwloc_bitmap_zero(cpus[i]);

  /* it_smt holds the intermediate SMT level */
  hw_smt = get_smt_level(topo);
  level_type = (usr_smt < hw_smt) ? HWLOC_OBJ_CORE : HWLOC_OBJ_PU;
  it_smt = (usr_smt > 1 && usr_smt < hw_smt) ? usr_smt : 0;

  /* If need to calculate nthreads, match at Core or usr-SMT level */
  if (*nthreads_ptr <= 0) {
    nobjs = hwloc_get_nbobjs_inside_cpuset_by_type(topo, root->cpuset,
						   level_type);
    if (it_smt)
      nobjs *= it_smt;

    /* Set the nthreads output */
    *nthreads_ptr = (nobjs >= ntasks) ? nobjs/ntasks : 1;
    /* Debug */
    //printf("num_threads=%d\n", *nthreads_ptr);
  }
  nwks = *nthreads_ptr * ntasks;

  /* If usr-SMT, I know exactly where to match */
  if ( usr_smt )
    get_pus_and_distribute(topo, root->cpuset,
			   hwloc_get_type_depth(topo, HWLOC_OBJ_CORE),
			   usr_smt, ntasks, cpus);
  else {
    /* Walk the tree to find a level big enough to support nwks */
    topo_depth = hwloc_topology_get_depth(topo);

    for (depth=root->depth; depth<topo_depth; depth++) {
      level_size =
	hwloc_get_nbobjs_inside_cpuset_by_depth(topo,
						root->cpuset, depth);
      if (level_size >= nwks || depth==topo_depth-1) {
	get_pus_and_distribute(topo, root->cpuset, depth,
			       1, ntasks, cpus);
	break;
      }
    }
  }
}

/*
 * Input:
 *   root: The root object to start from.
 *   ntasks: number of MPI tasks.
 *   nthreads: the number of threads per task. If zero,
 *      set the number of threads to match 'def_level.'
 *   def_level: if nthreads=0 (no user specified), match the
 *      tasks to this level, e.g., HWLOC_OBJ_CORE, HWLOC_OBJ_PU.
 * Output:
 *   cpus: array of one cpuset per task.
 */
void cpu_match_v4(hwloc_topology_t topo, hwloc_obj_t root, int ntasks,
		   int *nthreads_ptr, int usr_smt, hwloc_bitmap_t *cpus)
{
  int i, nwks, nobjs, core_depth, hw_smt, it_smt, nlevels;
  hwloc_obj_type_t level_type;
  struct topo_level *topo_levels;

  for (i=0; i<ntasks; i++)
    hwloc_bitmap_zero(cpus[i]);

  /* it_smt holds the intermediate SMT level */
  hw_smt = get_smt_level(topo);
  level_type = (usr_smt < hw_smt) ? HWLOC_OBJ_CORE : HWLOC_OBJ_PU;
  it_smt = (usr_smt > 1 && usr_smt < hw_smt) ? usr_smt : 0;

  /* If need to calculate nthreads, match at Core or usr-SMT level */
  if (*nthreads_ptr <= 0) {
    nobjs = hwloc_get_nbobjs_inside_cpuset_by_type(topo, root->cpuset,
						   level_type);
    if (it_smt)
      nobjs *= it_smt;

    /* Set the nthreads output */
    *nthreads_ptr = (nobjs >= ntasks) ? nobjs/ntasks : 1;
    /* Debug */
    //printf("num_threads=%d\n", *nthreads_ptr);
  }
  nwks = *nthreads_ptr * ntasks;

  /* This cpu_match function, while works, has the issue of
     potentially mapping tasks to overlapping cores, while
     the tasks could be mapped to cores that are not overlapping */

  /* Build the topology levels including intermediate SMT */
  topo_levels = build_mapping_levels(topo, root, &nlevels);
  /* Debug */
  char str[512];
  for (i=0; i<nlevels; i++) {
    hwloc_bitmap_list_snprintf(str, sizeof(str), topo_levels[i].cpuset);
    printf("level[%d]: %9s(%2d)-> %s\n", i, topo_levels[i].name,
	   topo_levels[i].nobjs, str);
  }

  core_depth = hwloc_get_type_depth(topo, HWLOC_OBJ_CORE);

  /* Walk the tree to find a level big enough to support nwks,
     or use the user SMT level */
  for (i=0; i<nlevels; i++)
    if ( (usr_smt && topo_levels[i].pus_per_obj == usr_smt &&
	  topo_levels[i].depth == core_depth)
	 || topo_levels[i].nobjs >= nwks || i==nlevels-1 ) {
      /* Distribute the pus over the ntasks and store in 'cpus' */
      fill_in_buckets_bitmap(topo_levels[i].cpuset, cpus, ntasks);
      break;
    }

  free_topo_levels(topo_levels, nlevels);
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
void cpu_match(hwloc_topology_t topo, hwloc_obj_t root, int ntasks,
		   int *nthreads_ptr, int usr_smt, hwloc_bitmap_t *cpus)
{
  int i, nwks, nobjs, hw_smt, it_smt, pus_per_obj;
  int depth, core_depth;
  hwloc_obj_t obj;
  hwloc_bitmap_t *cpuset;
  hwloc_obj_type_t level_type;
  char str[64];

  for (i=0; i<ntasks; i++)
    hwloc_bitmap_zero(cpus[i]);

  /* it_smt holds the intermediate SMT level */
  hw_smt = get_smt_level(topo);
  level_type = (usr_smt < hw_smt) ? HWLOC_OBJ_CORE : HWLOC_OBJ_PU;
  it_smt = (usr_smt > 1 && usr_smt < hw_smt) ? usr_smt : 0;

  /* If need to calculate nthreads, match at Core or usr-SMT level */
  if (*nthreads_ptr <= 0) {
    nobjs = hwloc_get_nbobjs_inside_cpuset_by_type(topo, root->cpuset,
						   level_type);
    if (it_smt)
      nobjs *= it_smt;

    /* Set the nthreads output */
    *nthreads_ptr = (nobjs >= ntasks) ? nobjs/ntasks : 1;
    /* Debug */
    //printf("num_threads=%d\n", *nthreads_ptr);
  }
  nwks = *nthreads_ptr * ntasks;

  /*
   * I need to always match at the Core level
   * if there are sufficient workers or smt is specified.
   * Then, if more resources are needed add pus per core as necessary.
   * The previous scheme of matching at smt-k or pu level does not
   * work as well because tasks may overlap cores--BFS vs DFS.
   */
  core_depth = get_core_depth(topo);

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
      }
      hwloc_obj_type_snprintf(str, sizeof(str), obj, 1);

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
      printf("Match: %s-%d depth %d nobjs %d npus %d nwks %d\n",
	     str, pus_per_obj, depth, nobjs, nobjs*pus_per_obj, nwks);

      /* Clean up */
      for (i=0; i<nobjs; i++)
	hwloc_bitmap_free(cpuset[i]);
      free(cpuset);

      break;
    }
  }
}

void topo_match(hwloc_topology_t topo, int numa_os_idx,
		int ntasks, int *nthreads_ptr,
		hwloc_bitmap_t *cpus, hwloc_bitmap_t gpus)
{
  hwloc_obj_t numa_obj = hwloc_get_numanode_obj_by_os_index(topo, numa_os_idx);

  /* HWLOC_OBJ_PU */
  //cpu_match(topo, numa_obj->parent, ntasks, nthreads_ptr,
  // HWLOC_OBJ_CORE, cpus);
  cpu_match(topo, numa_obj->parent, ntasks, nthreads_ptr,
	    HWLOC_OBJ_CORE, cpus);

  get_gpus(numa_obj->parent, gpus);
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
void gpu_match(hwloc_obj_t root, int ntasks, hwloc_bitmap_t *gpus_pt)
{
  hwloc_bitmap_t gpus;
  int i, devid, num_gpus;
  int *elems;

  /* Get the GPUs of this NUMA */
  gpus = hwloc_bitmap_alloc();
  num_gpus = get_gpus(root, gpus);

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

void gpu_match_v1(hwloc_obj_t root, int ntasks, hwloc_bitmap_t *gpus_pt)
{
  hwloc_bitmap_t gpus;
  int devid, idx, gpu_idx, task_idx, num_gpus, size;
  int *arr;

  /* Get the GPUs of this NUMA */
  gpus = hwloc_bitmap_alloc();
  num_gpus = get_gpus(root, gpus);

  /* Distribute GPUs among tasks */
  if (num_gpus > 0) {
    size = (ntasks >= num_gpus) ? ntasks : num_gpus;
    arr = calloc(size, sizeof(int));

    if (num_gpus >= ntasks) {
      /* Distribute num gpus over ntasks */
      distrib(num_gpus, ntasks, arr);
      /* Debug */
      //print_array(arr, ntasks);
      /* Each task gets at least one GPU */
      idx = 0;
      task_idx = 0;
      hwloc_bitmap_foreach_begin(devid, gpus) {
	/* Add one more gpu to task task_idx */
	hwloc_bitmap_set(gpus_pt[task_idx], devid);
	/* Num gpus for task task_idx: arr[task_idx] */
	if (++idx == arr[task_idx]) {
	  idx = 0;
	  task_idx++;
	}
      } hwloc_bitmap_foreach_end();

    } else {
      /* Distribute ntasks over num gpus */
      distrib(ntasks, num_gpus, arr);
      /* Debug */
      //print_array(arr, num_gpus);
      /* Each task gets only one GPU */
      gpu_idx = 0;
      task_idx = 0;
      hwloc_bitmap_foreach_begin(devid, gpus) {
	/* Num tasks for gpu gpu_idx: arr[gpu_idx] */
	for (idx=0; idx<arr[gpu_idx]; idx++)
	  /* Assign a single gpu to each task */
	  hwloc_bitmap_set(gpus_pt[task_idx++], devid);
	gpu_idx++;
      } hwloc_bitmap_foreach_end();
    }

    free(arr);
  }

  hwloc_bitmap_free(gpus);
}

void test_smt_info(hwloc_topology_t topo)
{
  hwloc_obj_t obj;
  char str[128];

  obj = NULL;
  while ( (obj = hwloc_get_next_obj_by_type(topo, HWLOC_OBJ_CORE, obj)) ) {
    hwloc_bitmap_list_snprintf(str, sizeof(str), obj->cpuset);
    printf("Core %d: %s\n", obj->os_index, str);
    print_obj_info(obj);
  }
}

/*
 * And then pass this as a parameter to this function
 * (this is my 'until' parameter from hwloc_distrib)
 * This is the core function that ties everything together!
 */
int mpibind_distrib(hwloc_topology_t topo, int ntasks, int nthreads,
		    int gpu_optim, int smt,
		    int *nthreads_pt,
		    hwloc_bitmap_t *cpus_pt,
		    hwloc_bitmap_t *gpus_pt)
{
  int i, j, num_numas, nt, np, task_offset;
  int *ntasks_per_numa;
  char str[128];
  hwloc_obj_t obj;
  hwloc_bitmap_t io_numa_os_ids;

  /* Distribute tasks over numa domains */
  if (gpu_optim) {
    io_numa_os_ids = hwloc_bitmap_alloc();
    num_numas = numas_wgpus(topo, io_numa_os_ids);
    /* Verbose */
    hwloc_bitmap_list_snprintf(str, sizeof(str), io_numa_os_ids);
    printf("io_numa_os_idxs: %s num: %d\n", str, num_numas);
  } else
    num_numas = hwloc_get_nbobjs_by_depth(topo, HWLOC_TYPE_DEPTH_NUMANODE);
  ntasks_per_numa = calloc(num_numas, sizeof(int));
  distrib(ntasks, num_numas, ntasks_per_numa);
  /* Verbose */
  print_array(ntasks_per_numa, num_numas, "ntasks_per_numa");

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
    // cpu_match(topo, obj->parent, np, &nt,
    //  HWLOC_OBJ_CORE, cpus_pt+task_offset);
    // cpu_match_smt(topo, obj->parent, np, &nt, smt, cpus_pt+task_offset);
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

/*
 * Give a single task the whole node even if it implies given
 * resources in more than one NUMA domain. This is useful for
 * Python-based programs and other ML workloads that have their
 * own parallelism but use a single process. Without this function
 * mpibind_distrib would map a single task to the resources
 * associated with a single NUMA domain.
 */
void greedy_singleton(hwloc_topology_t topo, int *nthreads, int usr_smt,
		      hwloc_bitmap_t cpus, hwloc_bitmap_t gpus)
{
  /* Get all the CPUs */
  //  cpu_match_v2(topo, hwloc_get_root_obj(topo), 1, nthreads,
  //	       HWLOC_OBJ_CORE, &cpus);
  cpu_match(topo, hwloc_get_root_obj(topo), 1, nthreads, usr_smt, &cpus);

  /* Debug */
  printf("nthreads=%d\n", *nthreads);

  /* Get all the GPUs */
  hwloc_obj_t obj = NULL;
  while ( (obj = hwloc_get_next_osdev(topo, obj)) != NULL )
    if (obj->attr->osdev.type == HWLOC_OBJ_OSDEV_COPROC)
      hwloc_bitmap_set(gpus, get_device_id(obj));
}

/*
 * Input:
 *   numa: numa domain from 0 to num_numas-1
 *    wks: number of workers
 * Output:
 *    pus: The cpuset to use for the input workers
 */
void level_match(hwloc_topology_t topology, int numa, int wks,
		 hwloc_bitmap_t pus) {
  hwloc_obj_t numa_obj, parent, obj;
  int i, depth, topodepth, num;
  char str[128];

  topodepth = hwloc_topology_get_depth(topology);
  numa_obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_NUMANODE, numa);
  parent = numa_obj->parent;
  //print_obj(parent);

  for (depth=parent->depth; depth<topodepth; depth++) {
    num = hwloc_get_nbobjs_inside_cpuset_by_depth(topology,
						  parent->cpuset, depth);
    if (num >= wks || depth==topodepth-1) {
      for (i=0; i<num; i++) {
	obj = hwloc_get_obj_inside_cpuset_by_depth(topology,
						   parent->cpuset, depth, i);
	print_obj(obj);
	//printf("pu: %d\n", hwloc_bitmap_first(obj->cpuset));
	hwloc_bitmap_set(pus, hwloc_bitmap_first(obj->cpuset));
      }
      hwloc_obj_type_snprintf(str, sizeof(str), obj, 1);
      break;
    }
  }

  printf("Match: %s depth=%d nobjs=%d\n", str, depth, num);
}

void match_io(hwloc_topology_t topology)
{
  int i, num_osdevs;
  hwloc_obj_t obj, io;

  num_osdevs = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_OS_DEVICE);
  printf("num_osdevs: %d\n", num_osdevs);
  for (i=0; i<num_osdevs; i++) {
    io = hwloc_get_obj_by_type(topology, HWLOC_OBJ_OS_DEVICE, i);
    if (io->attr->osdev.type == HWLOC_OBJ_OSDEV_COPROC) {
      print_obj(io);
      print_obj_info(io);

      /* Note that more than one GPU may belong to the same
	 ancestor, e.g., 2 GPUs per package */
      obj = hwloc_get_non_io_ancestor_obj(topology, io);
      print_obj(obj);
    }
  }
}

void match_and_print(hwloc_topology_t topology, int numa, int wks)
{
  hwloc_bitmap_t pus;
  char str[512];

  pus = hwloc_bitmap_alloc();
  level_match(topology, numa, wks, pus);
  hwloc_bitmap_list_snprintf(str, sizeof(str), pus);
  printf("pus: %s\n", str);
  hwloc_bitmap_free(pus);
}

void topo_match_and_print(hwloc_topology_t topo, int numa, int ntasks)
{
  int i, nthreads=0;
  char str[512];
  hwloc_bitmap_t cpus[ntasks], gpus;

  gpus = hwloc_bitmap_alloc();
  for (i=0; i<ntasks; i++)
    cpus[i] = hwloc_bitmap_alloc();

  topo_match(topo, numa, ntasks, &nthreads, cpus, gpus);

  for (i=0; i<ntasks; i++) {
    hwloc_bitmap_list_snprintf(str, sizeof(str), cpus[i]);
    printf("cpus[%d]: %s\n", i, str);
  }

  if (! hwloc_bitmap_iszero(gpus)) {
    hwloc_bitmap_list_snprintf(str, sizeof(str), gpus);
    printf("gpus: %s\n", str);
  }

  hwloc_bitmap_free(gpus);
  for (i=0; i<ntasks; i++)
    hwloc_bitmap_free(cpus[i]);
}

/*
 * Process the input and call the main mapping function.
 * Input:
 *   ntasks: number of tasks.
 *   nthreads: (optional) number of threads per task.
 *             Default value should be 0, in which case
 *             mpibind calculates the nthreads per task.
 *   greedy: Use the whole node when using a single task.
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
 *   size:
 *   nthreads:
 *   cpus:
 *   gpus:
 */
int mpibind(mpibind_in *in, mpibind_out *out)
{
  int i;
  hwloc_topology_t topo;
  unsigned version, major;

  /* hwloc API version 2 required */
  version = hwloc_get_api_version();
  major = version >> 16;
  if (major != 2) {
    printf("Error: hwloc API version 2 required. Current version %d.%d.%d\n",
	   major, (version>>8)&0xff, version&0xff);
    return 1;
  }

  /* Input parameters check */
  if (in->ntasks <= 0 || in->nthreads < 0) {
    printf("Error: Input ntasks/nthreads %d/%d out of range\n",
	   in->ntasks, in->nthreads);
    return 1;
  }

  /* Allocate space to store the resulting mapping */
  out->size = in->ntasks;
  out->nthreads = calloc(in->ntasks, sizeof(int));
  out->cpus = calloc(in->ntasks, sizeof(hwloc_bitmap_t));
  out->gpus = calloc(in->ntasks, sizeof(hwloc_bitmap_t));
  for (i=0; i<in->ntasks; i++) {
    out->cpus[i] = hwloc_bitmap_alloc();
    out->gpus[i] = hwloc_bitmap_alloc();
  }

  hwloc_topology_init(&topo);
  topology_load(topo);

  if (in->smt < 0 || in->smt > get_smt_level(topo)) {
    printf("Error: Input SMT parameter %d out of range\n", in->smt);
    return 1;
  }

  if (in->greedy && in->ntasks==1) {
    out->nthreads[0] = in->nthreads;
    greedy_singleton(topo, out->nthreads, in->smt,
		     out->cpus[0], out->gpus[0]);
  } else
    mpibind_distrib(topo, in->ntasks, in->nthreads,
		    in->gpu_optim, in->smt,
		    out->nthreads, out->cpus, out->gpus);

  hwloc_topology_destroy(topo);
  return 0;
}

void mpibind_free_v1(mpibind_out *out)
{
  int i;

  for (i=0; i<out->size; i++) {
    hwloc_bitmap_free(out->cpus[i]);
    hwloc_bitmap_free(out->gpus[i]);
  }
  free(out->cpus);
  free(out->gpus);
  free(out->nthreads);

#if 0
  if ( env ) {
    free(env->omp_num);
    free(env->omp_places);
    free(env->omp_policy);
    free(env->gpus);
  }
#endif
}

void mpibind_print(hwloc_topology_t topo,
		   int ntasks, int nthreads)
{
  int i;
  char str1[512], str2[512];
  int threads[ntasks];
  hwloc_bitmap_t cpus[ntasks], gpus[ntasks];

  for (i=0; i<ntasks; i++) {
    cpus[i] = hwloc_bitmap_alloc();
    gpus[i] = hwloc_bitmap_alloc();
  }

  int gpu_optim=1, smt=0;
  mpibind_distrib(topo, ntasks, nthreads,
		  gpu_optim, smt,
		  threads, cpus, gpus);

  for (i=0; i<ntasks; i++) {
    hwloc_bitmap_list_snprintf(str1, sizeof(str1), cpus[i]);
    hwloc_bitmap_list_snprintf(str2, sizeof(str2), gpus[i]);
    printf("task %2d: nths=%2d gpus=%s cpus=%s\n", i,
	   threads[i], str2, str1);
  }

  for (i=0; i<ntasks; i++) {
    hwloc_bitmap_free(cpus[i]);
    hwloc_bitmap_free(gpus[i]);
  }
}

void test_bitmap()
{
  hwloc_bitmap_t hola = hwloc_bitmap_alloc();
  hwloc_bitmap_set(hola, 20);
  hwloc_bitmap_set(hola, 0);
  hwloc_bitmap_set(hola, 3);
  int a = hwloc_bitmap_next(hola, -1);
  int b = hwloc_bitmap_next(hola, a);
  int c = hwloc_bitmap_next(hola, b);
  printf("a=%d b=%d c=%d\n", a, b, c);
}

void set_env_vars(mpibind_out *out, st_mp_out_env *env)
{
  int i, val, nc;
  char *str;
  /*Todo */
#if 0
  int nvars = 4;
  char *array[] = {"OMP_NUM_THREADS",
		   "OMP_PLACES",
		   "OMP_PROC_BIND",
		   "ROCR_VISIBLE_DEVICES"};
#endif

  /* Initialize/allocate env */
  env->size = out->size;
  env->omp_num = calloc(sizeof(st_mp_pair), out->size);
  env->omp_places = calloc(sizeof(st_mp_pair), out->size);
  env->omp_policy = calloc(sizeof(st_mp_pair), out->size);
  env->gpus = calloc(sizeof(st_mp_pair), out->size);

  for (i=0; i<out->size; i++) {
    snprintf(env->omp_num[i].name, sizeof(env->omp_num[i].name),
	     "OMP_NUM_THREADS");
    snprintf(env->omp_num[i].value, sizeof(env->omp_num[i].value),
	     "%d", out->nthreads[i]);

    snprintf(env->omp_places[i].name, sizeof(env->omp_places[i].name),
	     "OMP_PLACES");
    nc = 0;
    str = env->omp_places[i].value;
    hwloc_bitmap_foreach_begin(val, out->cpus[i]) {
      nc += snprintf(str+nc, sizeof(str), "{%d},", val);
    } hwloc_bitmap_foreach_end();

    snprintf(env->omp_policy[i].name, sizeof(env->omp_policy[i].name),
	     "OMP_PROC_BIND");
    snprintf(env->omp_policy[i].value, sizeof(env->omp_policy[i].value),
	     "spread");

    if (out->gpu_type == MPIBIND_GPU_TYPE_AMD)
      snprintf(env->gpus[i].name, sizeof(env->gpus[i].name),
	       "ROCR_VISIBLE_DEVICES");
    else if (out->gpu_type == MPIBIND_GPU_TYPE_NVIDIA)
      snprintf(env->gpus[i].name, sizeof(env->gpus[i].name),
	       "CUDA_VISIBLE_DEVICES");
    else
      printf("Something is wrong\n");
    nc = 0;
    str = env->gpus[i].value;
    hwloc_bitmap_foreach_begin(val, out->gpus[i]) {
      nc += snprintf(str+nc, sizeof(str), "%d,", val);
    } hwloc_bitmap_foreach_end();

  }
}

/*
  Section 3.3
    Hierarchy, Tree and Levels
  Section 7.3
    Co-Processors (HWLOC_OBJ_OSDEV_COPROC)
    OpenFabrics (InfiniBand, Omni-Path, usNIC, etc) HCAs
     (HWLOC_OBJ_OSDEV_OPENFABRICS)
     – mlx5_0, hfi1_0, qib0, usnic_0 (Linux component)
  Section 7.5
    hwloc_get_non_io_ancestor_obj()
  Chapter 9
    Object attributes
  Section 9.2
    Custom string infos
    hwloc_obj_get_info_by_name()
    hwloc_obj_add_info()
  Section 17.1.4
    Finding Local NUMA nodes and looking at Children and Parents
  Section 17.2.2
    Kinds of objects
    hwloc_obj_type_is_normal(), hwloc_obj_type_is_memory(),
    Normal, Memory, I/O, Misc
  Section 22.14.2.3
    hwloc_get_nbobjs_inside_cpuset_by_depth()
  Section 22.3
    Object Types
  Section 22.5
    Topology Creation and Destruction
    See also hwloc-2.2.0/tests/hwloc/hwloc_topology_dup.c
  Section 22.7
    Converting between Object Types and Attributes, and Strings
  Section 22.11
    Changing the Source of Topology Discovery
    Topology components, e.g., cuda
  22.12
    Topology Detection Configuration and Query
  Section 22.19
    Finding objects, miscellaneous helpers
    22.19.2.1
    hwloc_bitmap_singlify_per_core()
  Section 22.35
  Interoperability with the CUDA Driver API
  Section 22.41
    Sharing topologies between processes
    These functions are used to share a topology between processes by
    duplicating it into a file-backed shared-memory buffer.
  Section 22.45
    Components and Plugins: Core functions to be used by components
  ** Adding custom objects to the topology (e.g., SMT-2)
  See topology-cuda.c: hwloc_cuda_discover(...)
      topology-opencl.c: hwloc_opencl_discover(...)

  int hwloc_bitmap_list_sscanf(hwloc_bitmap_t bitmap,
  const char ∗restrict string)
  Parse a list string and stores it in bitmap bitmap.
 */

int main(int argc, char *argv[]) {
  hwloc_topology_t topology;
  int i, num_numas;
  hwloc_obj_t root, obj;
  char *str;
  char string[128];

  /* Todo: need another wrapper that sets:
     omp_places, omp_proc_bind, *visible_devices */

#if 1
  printf("=====Begin main mpibind\n");
  mpibind_in params;
  mpibind_out mapping;
  char str1[256], str2[256];
  params.ntasks = 1;
  params.nthreads = 10;
  params.greedy = 1;
  params.gpu_optim = 1;
  params.smt = 3;

  mpibind(&params, &mapping);

  for (i=0; i<params.ntasks; i++) {
    hwloc_bitmap_list_snprintf(str1, sizeof(str1), mapping.cpus[i]);
    hwloc_bitmap_list_snprintf(str2, sizeof(str2), mapping.gpus[i]);
    printf("task %2d: nths=%2d gpus=%s cpus=%s\n", i,
	   mapping.nthreads[i], str2, str1);
  }

#if 0
  mpibind_set_env_vars(&mapping, &env);
  for (i=0; i<env.size; i++) {
    printf("task %2d: %s: %s\n", i,
	   env.omp_num[i].name, env.omp_num[i].value);
    printf("task %2d: %s: %s\n", i,
	   env.omp_places[i].name, env.omp_places[i].value);
    printf("task %2d: %s: %s\n", i,
	   env.omp_policy[i].name, env.omp_policy[i].value);
    printf("task %2d: %s: %s\n", i,
	   env.gpus[i].name, env.gpus[i].value);

  }
#endif

  mpibind_free_v1(&mapping);
  printf("=====End main mpibind\n");
  return 0;
#endif

  //test_bitmap();

  //test_buckets();

  hwloc_topology_init(&topology);  // initialization
  topology_load(topology);

  printf("hwloc version 0x%x\n", hwloc_get_api_version());

  root = hwloc_get_root_obj(topology);
  print_obj(root);

  printf("=====Begin hwloc_distrib\n");
  hwloc_bitmap_t set[4];
  for (i=0; i<4; i++)
    set[i] = hwloc_bitmap_alloc();
  hwloc_distrib(topology, &root, 1, set, 4, 2, 0);
  for (i=0; i<4; i++) {
    hwloc_bitmap_list_snprintf(string, sizeof(string), set[i]);
    printf("[%d]: %s\n", i, string);
    hwloc_bitmap_free(set[i]);
  }
  printf("=====End hwloc_distrib\n");

  // For each object, print this value
  // Need to know how this works...
  hwloc_bitmap_asprintf(&str, root->nodeset);
  printf("%s\n", str);

  /*
    For the 1-mpi task case, use hwloc_distrib and done!
    static int hwloc_distrib (hwloc_topology_t topology,
    hwloc_obj_t ∗ roots, unsigned n_roots,
    hwloc_cpuset_t ∗ set, unsigned n, int until,
    unsigned long flags)
  */

  /* num_numas = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_NUMANODE);  */
  /* //printf("%u NUMAs\n", num_numas); */
  /* /\* By depth is preferred for NUMA and I/O nodes *\/ */
  /* // HWLOC_TYPE_DEPTH_NUMANODE, HWLOC_TYPE_DEPTH_OS_DEVICE */
  /* printf("%u NUMAs\n", */
  /* 	 hwloc_get_nbobjs_by_depth(topology, HWLOC_TYPE_DEPTH_NUMANODE)); */
  /* for (i=0; i<num_numas; i++) { */
  /*   obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_NUMANODE, i); */
  /*   print_obj(obj);  */
  /*   if (obj->parent) { */
  /*     hwloc_obj_type_snprintf(string, sizeof(string), obj->parent, 0); */
  /*     printf("Parent %s %d %d\n", string, obj->parent->logical_index, */
  /* 	     obj->parent->arity);  */
  /*   } */
  /* } */

  printf("=====GPU match begin\n");
  int num_tasks = 1;
  hwloc_bitmap_t gpus_pt[num_tasks];
  for (i=0; i<num_tasks; i++)
    gpus_pt[i] = hwloc_bitmap_alloc();

  hwloc_obj_t numa_obj = hwloc_get_numanode_obj_by_os_index(topology, 0);
  print_obj(numa_obj->parent);
  gpu_match(numa_obj->parent, num_tasks, gpus_pt);

  for (i=0; i<num_tasks; i++) {
    hwloc_bitmap_list_snprintf(string, sizeof(string), gpus_pt[i]);
    printf("[%d]: %s\n", i, string);
    hwloc_bitmap_free(gpus_pt[i]);
  }
  printf("=====GPU match end \n");

  hwloc_bitmap_t numa_os_idxs2 = hwloc_bitmap_alloc();
  printf("=====Mem Begin\n");
  obj = NULL;
  while ( (obj = hwloc_get_next_obj_by_depth(topology, HWLOC_TYPE_DEPTH_NUMANODE, obj)) != NULL )
    hwloc_bitmap_set(numa_os_idxs2, obj->os_index);
  num_numas = hwloc_bitmap_weight(numa_os_idxs2);
  hwloc_bitmap_list_snprintf(string, sizeof(string), numa_os_idxs2);
  printf("%d numas: %s\n", num_numas, string);
  printf("=====Mem End\n");

  //match_and_print(topology, 1, 5);
  printf("=====Match Begin\n");
  hwloc_bitmap_foreach_begin(i, numa_os_idxs2) {
    topo_match_and_print(topology, i, 6);
  } hwloc_bitmap_foreach_end();
  printf("=====Match End\n");
  hwloc_bitmap_free(numa_os_idxs2);

  //  match_io(topology);
  printf("=====IO Begin\n");
  char str3[128];
  hwloc_bitmap_t numa_os_idxs = hwloc_bitmap_alloc();
  numas_wgpus(topology, numa_os_idxs);
  hwloc_bitmap_list_snprintf(str3, sizeof(str3), numa_os_idxs);
  printf("numa_os_idxs: %s\n", str3);
  obj = hwloc_get_numanode_obj_by_os_index(topology,
					   hwloc_bitmap_first(numa_os_idxs));
  //parent = numa_obj->parent;
  //printf("Parent of NUMA:\n");
  print_obj(obj->parent);
  hwloc_bitmap_free(numa_os_idxs);
  printf("=====IO End\n");

  printf("=====mpibind Match Begin\n");
  mpibind_print(topology, 10, 0);
  printf("=====mpibind Match End\n");

  //  get_smt_level(topology);
  //add_smt_info(topology);
  test_smt_info(topology);
#if 0
  /* Adding intermediate levels between Core and PU
     may be too complicated, since the topology needs
     to be update accordingly: depths, parents, children, etc. */
  printf("======Topology begin\n");
  hwloc_obj_t newobj = hwloc_alloc_setup_object(topology, HWLOC_OBJ_GROUP,
						172);
  //HWLOC_UNKNOWN_INDEX);
  newobj->cpuset = hwloc_bitmap_alloc();
  hwloc_bitmap_set(newobj->cpuset, 172);
  hwloc_bitmap_set(newobj->cpuset, 173);
  hwloc_bitmap_set(newobj->cpuset, 174);
  hwloc_insert_object_by_cpuset(topology, newobj);
  print_obj(newobj->parent);
  walk_the_cores(topology);
  //print_children(topology, hwloc_get_root_obj(topology), 0);
  printf("======Topology end\n");
#endif

  hwloc_topology_destroy(topology);

  return 0;
}
