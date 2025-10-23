/******************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory
 ******************************************************/
#include <hwloc.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "mpibind.h"
#include "mpibind-priv.h"
#include "hwloc_utils.h"

/*
 * Todo list:
 *
 * test-suite:
 *   On Mac, the test-suite does not run succesfully
 *   because the full path of the libtap library
 *   is not present in the library dependencies
 *   of the binary.
 *
 * github repo:
 *   Tag mpibind versions. Look at the commit history
 *   to write the release notes.
 *   Include an example of how to use mpibind in README
 *   If src/main.c grows significantly, create an examples
 *   directory with different focal points, e.g.,
 *   dealing with GPU IDs.
 */

/************************************************
 * Functions defined in internals.c
 * or hwloc_utils.c
 ************************************************/
int get_smt_level(hwloc_topology_t topo);
int discover_devices(hwloc_topology_t topo,
      struct device **devs, int size);
int get_num_gpus(struct device **devs, int ndevs);
int mpibind_distrib(hwloc_topology_t topo,
      struct device **devs, int ndevs,
		  int ntasks, int nthreads,
		  int greedy, int gpu_optim, int smt,
		  int *nthreads_pt,
		  hwloc_bitmap_t *cpus_pt,
		  hwloc_bitmap_t *gpus_pt);
int device_key_snprint(char *buf, size_t size,
      struct device *dev, int id_type);
int get_gpu_vendor_id(struct device **devs, int ndevs);
char* get_gpu_vendor(struct device **devs, int ndevs);
const hwloc_bitmap_t get_core_cpuset(hwloc_topology_t topo, int pu);
void terminate_str(char *buf, int size);
int filter_topology(hwloc_topology_t topology);
int numas_have_intersecting_cpus(hwloc_topology_t topo);
int restrict_numas_with_intersecting_cpus(hwloc_topology_t topo);
int check_topology(hwloc_topology_t topo);

/*********************************************
 * Public interface of mpibind.
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

  hdl->ndevs = 0;
  hdl->devs = NULL;

  /* Initialize output parameters */
  hdl->nthreads = NULL;
  hdl->cpus = NULL;
  hdl->gpus = NULL;
  hdl->gpus_usr = NULL;
  hdl->cpus_usr = NULL;

  *handle = hdl;

  return 0;
}

/*
 * Release resources associated with the input handle.
 * After this call, no mpibind function should be called.
 */
int mpibind_finalize(mpibind_t *hdl)
{
  int i, j, v;

  if (hdl == NULL)
    return 1;

  /* Release user-specified GPU ID type */
  if (hdl->gpus_usr != NULL) {
    for (i=0; i<hdl->ntasks; i++) {
      for (j=0; j<hwloc_bitmap_weight(hdl->gpus[i]); j++)
        free(hdl->gpus_usr[i][j]);
      free(hdl->gpus_usr[i]);
    }
    free(hdl->gpus_usr);
  }

  /* Release CPU arrays */
  if (hdl->cpus_usr != NULL) {
    for (i=0; i<hdl->ntasks; i++) {
      free(hdl->cpus_usr[i]);
    }
    free(hdl->cpus_usr);
  }

  /* Release mapping space */
  for (i=0; i<hdl->ntasks; i++) {
    hwloc_bitmap_free(hdl->cpus[i]);
    hwloc_bitmap_free(hdl->gpus[i]);
  }
  free(hdl->cpus);
  free(hdl->gpus);
  free(hdl->nthreads);

  /* Release I/O devices structure */
  for (i=0; i<hdl->ndevs; i++)
    free(hdl->devs[i]);
  free(hdl->devs);

  /* Release env variables space */
  for (v=0; v<hdl->nvars; v++) {
#if VERBOSE >= 3
    PRINT("Releasing %s\n", hdl->env_vars[v].name);
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

  /* Release the main structure */
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
 *
 * Get the GPU mapping.
 * This function returns the GPU IDs based on
 * mpibind's internal assignment of IDs,
 * rather than VISIBLE_DEVICES IDs.
 * To use these other type of IDs, use functions
 * mpibind_set_gpu_ids() and mpibind_get_gpus_ptask().
 *
 * With mpibind IDs one can obtain other type of IDs
 * such as VISIBLE_DEVICES or PCI BUSID. This is
 * really an internal function but I am making it
 * user visible for completeness with
 * mpibind_get_cpus(). In the future, I may add
 * another function that translates an mpibind ID
 * to another type of ID such as PCI BUSID, so that
 * the user has the flexibility to translate
 * individual IDs.
 */
hwloc_bitmap_t* mpibind_get_gpus(mpibind_t *handle)
{
  if (handle == NULL)
    return NULL;

  return handle->gpus;

#if 0
  /* Example of translating mpibind IDs to
     VIS_DEVS IDs */
  int i, val;
  hwloc_bitmap_t *arr = calloc(handle->ntasks,
                              sizeof(hwloc_bitmap_t));
  for (i=0; i<handle->ntasks; i++) {
    arr[i] = hwloc_bitmap_alloc();
    hwloc_bitmap_zero(arr[i]);
    hwloc_bitmap_foreach_begin(val, handle->gpus[i]) {
      hwloc_bitmap_set(arr[i], handle->devs[val]->visdevs);
	  } hwloc_bitmap_foreach_end();
  }
  return arr;
#endif
}

/*
 * Get an array of strings with the GPU IDs of
 * the specified task. The type of ID used can
 * be specified by the user with
 * mpibind_set_gpu_ids()
 * The default ID type is MPIBIND_ID_SMI.
 */
char ** mpibind_get_gpus_ptask(mpibind_t *handle, int taskid,
                               int *ngpus)
{
  if (handle == NULL || taskid >= handle->ntasks || taskid < 0)
    return NULL;

  if (handle->gpus_usr == NULL)
    /* User hasn't called mpibind_set_gpu_ids().
       Call this function with the default ID type */
    mpibind_set_gpu_ids(handle, MPIBIND_ID_SMI);

  *ngpus = hwloc_bitmap_weight(handle->gpus[taskid]);

  return handle->gpus_usr[taskid];
}

int* mpibind_get_cpus_ptask(mpibind_t *handle, int taskid,
			    int *ncpus)
{
  if (handle == NULL || taskid >= handle->ntasks || taskid < 0)
    return NULL;

  *ncpus = hwloc_bitmap_weight(handle->cpus[taskid]);

  return handle->cpus_usr[taskid];
}

/*
 * Get the number of GPUs in the system/allocation.
 */
int mpibind_get_num_gpus(mpibind_t *handle)
{
  if (handle == NULL)
    return -1;

  return get_num_gpus(handle->devs, handle->ndevs);
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
 * Wrap hwloc_topology_load with extra filters
 * to make sure the topology meets the criteria
 * for mpibind's correct operation.
 *
 * Return 0 on success and 1 otherwise
 */
int mpibind_load_topology(hwloc_topology_t topo)
{
  /* Make sure OS functions are actually called
     when binding workers. Could also use HWLOC_THISSYSTEM=1,
     but that applies to all hwloc clients */
  if (hwloc_topology_set_flags(topo, HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM) < 0)
    PRINT("WARN: OS binding may not be enforced\n");

  /* Make sure OS and PCI devices are not filtered out */
  if (filter_topology(topo) < 0)
    PRINT("WARN: Failed to incorporate key topology components\n");

  if (hwloc_topology_load(topo) < 0) {
    PRINT("ERR: hwloc_topology_load failed\n");
    return 1;
  }

#if VERBOSE >= 1
  print_topo_brief(topo);
#endif

  /* Remove NUMA domains with intersecting CPUs */
  if ( numas_have_intersecting_cpus(topo) )
    restrict_numas_with_intersecting_cpus(topo);

  return 0;
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
 *   nthreads, cpus, gpus.
 * Returns 0 on success.
 */
int mpibind(mpibind_t *hdl)
{
  int i, j, val, gpu_optim, rc=0;
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
    mpibind_load_topology(hdl->topo);
  } else
    /* Caller provides the hwloc topology */
    check_topology(hdl->topo);

#if VERBOSE >= 1
  print_topo_brief(hdl->topo);
  print_topo_io(hdl->topo);
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

    if ( hwloc_topology_restrict(hdl->topo, set, flags) )
      PRINT("Warn: Failed to restrict topology to %s\n", hdl->restr_set);

#if VERBOSE >= 1
    PRINT("Restricted topology to %s with flags %lu\n",
	  hdl->restr_set, flags);
#endif

    hwloc_bitmap_free(set);
  }

  /* Discover I/O devices */
  hdl->devs = calloc(MAX_IO_DEVICES, sizeof(struct device *));
  for (i=0; i<MAX_IO_DEVICES; i++)
    hdl->devs[i] = NULL;
  hdl->ndevs = discover_devices(hdl->topo, hdl->devs, MAX_IO_DEVICES);

#if VERBOSE >=1
  PRINT("Effective I/O devices: %d\n", hdl->ndevs);
  for (i=0; i<hdl->ndevs; i++)
    PRINT("[%d]: busid=%s smi=%d name=%s\n"
	  "\tvendor=%s model=%s\n"
	  "\tuuid=%s\n"
	  "\tvendorid=0x%x ancestor=0x%" PRIu64 "\n",
	  i, hdl->devs[i]->pci, hdl->devs[i]->smi, hdl->devs[i]->name,
	  hdl->devs[i]->vendor, hdl->devs[i]->model,
	  hdl->devs[i]->univ, hdl->devs[i]->vendor_id,
	  hdl->devs[i]->ancestor->gp_index);
#endif

  /* If there's no GPUs, mapping should be CPU-guided */
  gpu_optim = ( get_num_gpus(hdl->devs, hdl->ndevs) ) ? 1 : 0;
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
  PRINT("Input: tasks %d threads %d greedy %d smt %d\n",
	hdl->ntasks, hdl->in_nthreads, hdl->greedy, hdl->smt);
  PRINT("GPUs: count %d optim %d vendor %s\n",
	get_num_gpus(hdl->devs, hdl->ndevs), gpu_optim,
	get_gpu_vendor(hdl->devs, hdl->ndevs));
#endif

  /* Calculate the mapping.
     I could pass the mpibind handle, but using explicit
     parameters for now. */
  rc = mpibind_distrib(hdl->topo, hdl->devs, hdl->ndevs,
		       hdl->ntasks, hdl->in_nthreads,
		       hdl->greedy, gpu_optim, hdl->smt,
		       hdl->nthreads, hdl->cpus, hdl->gpus);

  /* Finally, populate hdl->cpus_usr */
  hdl->cpus_usr = calloc(hdl->ntasks, sizeof(int *));
  for (i=0; i<hdl->ntasks; i++) {
    hdl->cpus_usr[i] = calloc(MAX_CPUS_PER_TASK, sizeof(int));
    j = 0;
    hwloc_bitmap_foreach_begin(val, hdl->cpus[i]) {
      hdl->cpus_usr[i][j++] = val;
    } hwloc_bitmap_foreach_end();
  }

  /* Don't destroy the topology, because the caller may
     need it to parse the resulting cpu/gpu bitmaps */
  //hwloc_topology_destroy(topo);

  return rc;
}

/*
 * Pop 'ncpus' CPUs from the assigned CPUs of a particular task.
 * The main 'mpibind' function has to be called before calling
 * this function.
 */
int mpibind_pop_cpus_ptask(mpibind_t *handle, int taskid, int ncpus)
{
  if (handle == NULL ||
      taskid < 0 || taskid >= handle->ntasks ||
      ncpus < 1)
    return -1;

  int i, val;
  hwloc_bitmap_t cpuset = handle->cpus[taskid];
  int weight = hwloc_bitmap_weight(cpuset);

  if (weight <= ncpus) {
    fprintf(stderr, "mpibind_pop_cpus_ptask: "
	    "Popping %d CPUs would leave task %d with no CPUs\n",
	    ncpus, taskid);
    return -1;
  }

  /* Update cpus */
  for (i=0; i<ncpus; i++)
    hwloc_bitmap_clr(cpuset, hwloc_bitmap_first(cpuset));

  /* Update cpus_usr */
  i=0;
  hwloc_bitmap_foreach_begin(val, cpuset) {
    handle->cpus_usr[taskid][i++] = val;
  } hwloc_bitmap_foreach_end();

  /* Update nthreads */
  handle->nthreads[taskid] = hwloc_bitmap_weight(cpuset);

  return 0;
}

/*
 * Pop 'ncpus' CPUs from the assigned CPUs of a particular task.
 * The main 'mpibind' function has to be called before calling
 * this function.
 */
int mpibind_pop_cores_ptask(mpibind_t *handle, int taskid, int ncores)
{
  if (handle == NULL ||
      taskid < 0 || taskid >= handle->ntasks ||
      ncores < 1)
    return -1;

  int i, pu, val;
  hwloc_bitmap_t cpuset = handle->cpus[taskid];
  int weight = hwloc_bitmap_weight(cpuset);
  hwloc_topology_t topo = mpibind_get_topology(handle);

  if (weight <= ncores) {
    fprintf(stderr, "mpibind_pop_cores_ptask: "
	    "Popping %d CPUs would leave task %d with no CPUs\n",
	    ncores, taskid);
    return -1;
  }

  /* Update cpus */
  for (i=0; i<ncores; i++) {
    pu = hwloc_bitmap_first(cpuset);
    hwloc_bitmap_andnot(cpuset, cpuset, get_core_cpuset(topo, pu));
  }

  /* Update cpus_usr */
  i=0;
  hwloc_bitmap_foreach_begin(val, cpuset) {
    handle->cpus_usr[taskid][i++] = val;
  } hwloc_bitmap_foreach_end();

  /* Update nthreads */
  handle->nthreads[taskid] = hwloc_bitmap_weight(cpuset);

  return 0;
}

/*
 * Print the mapping for a given task to a string.
 */
int mpibind_mapping_ptask_snprint(char *buf, size_t size,
                                  mpibind_t *handle, int taskid)
{
  if (handle == NULL || taskid < 0 || taskid >= handle->ntasks)
    return -1;

  int j, nc=0;
  /* The number of threads */
  nc += snprintf(buf+nc, size-nc, "mpibind: task %3d nths %2d gpus ",
                 taskid, handle->nthreads[taskid]);

  /* The GPUs */
  if (handle->gpus_usr == NULL) {
    /* The user did not specify the type of IDs to use:
       Use the mpibind IDs */
    if (size <= nc)
      return nc;
    nc += hwloc_bitmap_list_snprintf(buf+nc, size-nc, handle->gpus[taskid]);
  } else {
    /* Use the user-specified IDs (stored in gpus_usr) */
    for (j=0; j<hwloc_bitmap_weight(handle->gpus[taskid]); j++) {
      if (size <= nc)
	return nc;
      nc += snprintf(buf+nc, size-nc, "%s,", handle->gpus_usr[taskid][j]);
    }
    if (buf[nc-1] == ',')
      nc--;
  }

  /* The CPUs */
  if (size <= nc)
    return nc;
  nc += snprintf(buf+nc, size-nc, " cpus ");
  if (size > nc)
    nc += hwloc_bitmap_list_snprintf(buf+nc, size-nc, handle->cpus[taskid]);

#if DEBUG >= 1
  fprintf(OUT_STREAM, "mapping_ptask: task=%d size=%lu nc=%d\n",
	  taskid, size, nc);
#endif

  return nc;
}

/*
 * Print the mapping to a string.
 */
int mpibind_mapping_snprint(char *buf, size_t size,
                            mpibind_t *handle)
{
  if (handle == NULL)
    return -1;

  int i, nc=0;

  for (i=0; i<handle->ntasks; i++) {
    if (size <= nc) {
      terminate_str(buf, size);
      return nc;
    }
    nc += mpibind_mapping_ptask_snprint(buf+nc, size-nc, handle, i);

    if (size <= nc) {
      terminate_str(buf, size);
      return nc;
    }
    nc += snprintf(buf+nc, size-nc, "\n");

    // snprintf writes the terminating null byte '\0'
    // A return value of size or more means the output was truncated
    // Debug:
    // fprintf(stderr, "mapping: task=%d size=%lu nc=%d\n", i, size, nc);
  }

  return nc;
}

/*
 * Print the mapping.
 */
void mpibind_mapping_print(mpibind_t *handle)
{
  if (handle == NULL)
    return;

  char str[handle->ntasks * LONG_STR_SIZE];
  mpibind_mapping_snprint(str, sizeof(str), handle);
  printf("%s\n", str);
}

/*
 * Specify the type of IDs to use when retrieving
 * GPUs from the mpibind mapping, e.g., MPIBIND_ID_PCIBUS.
 * mpibind() must precede this function. The default
 * ID type is MPIBIND_ID_VISDEVS.
 * This function returns 0 on success and 1 otherwise.
 */
int mpibind_set_gpu_ids(mpibind_t *handle, int id_type)
{
  int val, i, j, ngpus;

  if (handle == NULL ||
      (id_type != MPIBIND_ID_NAME &&
      id_type != MPIBIND_ID_PCIBUS &&
      id_type != MPIBIND_ID_SMI &&
      id_type != MPIBIND_ID_UNIV))
    return 1;

  /* If space has not been allocated, allocate space
     to store GPU mapping. Space should be allocated
     only once */
  if (handle->gpus_usr == NULL) {
    handle->gpus_usr = calloc(handle->ntasks, sizeof(char **));
    for (i=0; i<handle->ntasks; i++) {
      ngpus = hwloc_bitmap_weight(handle->gpus[i]);
      handle->gpus_usr[i] = calloc(ngpus, sizeof(char *));
      for (j=0; j<ngpus; j++)
        /* UUID_LEN is the largest size */
        handle->gpus_usr[i][j] = calloc(UUID_LEN, sizeof(char));
    }
  }

  for (i=0; i<handle->ntasks; i++) {
    j = 0;
    hwloc_bitmap_foreach_begin(val, handle->gpus[i]) {
      device_key_snprint(handle->gpus_usr[i][j],
        UUID_LEN, handle->devs[val], id_type);
      j++;
	  } hwloc_bitmap_foreach_end();
  }

#if VERBOSE >= 2
  for (i=0; i<handle->ntasks; i++)
    for (j=0; j<hwloc_bitmap_weight(handle->gpus[i]); j++)
      PRINT("Task[%d] GPU[%d]: %s\n", i, j,
	    handle->gpus_usr[i][j]);
#endif

  return 0;
}

/*
 * Environment variables that need to be exported by the runtime.
 * CUDA_VISIBLE_DEVICES --comma separated
 * ROCR_VISIBLE_DEVICES
 * OMP_NUM_THREADS
 * OMP_PLACES --comma separated, each item in curly braces.
 * OMP_PROC_BIND --spread
 *
 * Todo: Use UUIDs instead of GPU indices to restrict
 * the topology with VISIBLE_DEVICES. I cannot
 * proceed because ROCR_VISIBLE_DEVICES doesn't
 * support UUIDs even though ROCm does:
 *   rocm-smi --showuniqueid --showbus
 * CUDA_VISIBLE_DEVICES does support UUIDs.
 */
int mpibind_set_env_vars(mpibind_t *handle)
{
  int i, v, nc, val, end, vendor;
  char *str;
  const char *vars[] = {
    "OMP_NUM_THREADS",
    "OMP_PLACES",
    "OMP_PROC_BIND",
    "VISIBLE_DEVICES"
  };
  int nvars = sizeof(vars) / sizeof(const char *);

  if (handle == NULL)
    return 1;

  /* Initialize/allocate env */
  handle->nvars = nvars;
  handle->env_vars = calloc(nvars, sizeof(mpibind_env_var));

  vendor = get_gpu_vendor_id(handle->devs, handle->ndevs);

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
	/*
	 * Simplifying the value of this variable from
	 * a list of PUs to 'threads'.
	 *
	 * mpibind binds each process to a set of PUs already
	 * so it might be redundant to apply the thread
	 * binding to the same list of PUs as the process
	 * binding.
	 * While setting this env variable to a explicit
	 * list of places should not hurt, it can be problematic
	 * for some OpenMP compilers that interpret the list
	 * of places as relative IDs: An ID of 4 does not mean
	 * hardware thread 4, instead it means the forth place.
	 * This may result in errors when passing large IDs
	 * like 60 (hardware thread 60) since there may not be a
	 * 60th place within this process.
	 */
#if 0
	nc = 0;
	hwloc_bitmap_foreach_begin(val, handle->cpus[i]) {
	  nc += snprintf(str+nc,
			 (LONG_STR_SIZE-nc < 0) ? 0 : LONG_STR_SIZE-nc,
			 "{%d},", val);
	} hwloc_bitmap_foreach_end();
#else
	snprintf(str, LONG_STR_SIZE, "threads");
#endif
      }

      else if ( strncmp(vars[v], "OMP_PROC", 8) == 0 )
	snprintf(str, LONG_STR_SIZE, "spread");

      else if ( strncmp(vars[v], "VISIBLE_DEVICES", 8) == 0 ) {
	if (vendor == 0x1002)
	  snprintf(handle->env_vars[v].name, SHORT_STR_SIZE,
		   "ROCR_VISIBLE_DEVICES");
	else if (vendor == 0x10de)
	  snprintf(handle->env_vars[v].name, SHORT_STR_SIZE,
		   "CUDA_VISIBLE_DEVICES");
	nc = 0;
        /* Use the GPU's visible devices ID (visdevs),
           not the mpibind ID (val).
           Todo: When AMD supports UUIDs, use UUIDs instead */
	hwloc_bitmap_foreach_begin(val, handle->gpus[i]) {
	  nc += snprintf(str+nc, LONG_STR_SIZE-nc, "%d,",
			 handle->devs[val]->smi);
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

void mpibind_env_vars_print(mpibind_t *handle)
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

int mpibind_apply(mpibind_t *handle, int taskid)
{
  int rc = -1;
  hwloc_bitmap_t *core_sets = mpibind_get_cpus(handle);
  hwloc_topology_t topo = mpibind_get_topology(handle);

  if (topo && core_sets) {
    rc = 0;
    if ((rc = hwloc_set_cpubind(topo, core_sets[taskid], 0)) < 0)
      perror("hwloc_set_cpubind");
  }

  return rc;
}

/*
 * mpibind relies on Core objects. If the topology
 * doesn't have them, use an appropriate replacement.
 * Make sure to always use get_core_type and get_core_depth
 * instead of HWLOC_OBJ_CORE and its depth.
 * Todo: In the future, I may need to have similar functions
 * for NUMA domains.
 */
int mpibind_get_core_depth(hwloc_topology_t topo)
{
  return hwloc_get_type_or_below_depth(topo, HWLOC_OBJ_CORE);
}

hwloc_obj_type_t mpibind_get_core_type(hwloc_topology_t topo)
{
  return hwloc_get_depth_type(topo, mpibind_get_core_depth(topo));
}

/*
 * Restrict the topology to the current binding.
 *
 * When Slurm provides an allocation consisiting of a
 * subset of cores, the resulting topology may include
 * cpuless NUMAs/Groups objects. Restricting the
 * topology with the remove_cpuless flag addresses this
 * issue so that mpibind can provide a valid mapping.
 *
 * This function also renumbers the logical ids of cores,
 * which is helpful in the context of Flux in nested jobs.
 */
int mpibind_restrict_to_current_binding(hwloc_topology_t topo)
{
  hwloc_bitmap_t rset = NULL;
  int rc = -1;

  if ( !(rset = hwloc_bitmap_alloc()) )
    return rc;

  if (hwloc_get_cpubind(topo, rset, HWLOC_CPUBIND_PROCESS) < 0)
    goto out;
#if VERBOSE >= 1
  else {
    char str[SHORT_STR_SIZE];
    hwloc_bitmap_list_snprintf(str, sizeof(str), rset);
    PRINT("Process cpubind: <%s>\n", str);
  }
#endif

  if (hwloc_topology_restrict(topo, rset,
			      HWLOC_RESTRICT_FLAG_REMOVE_CPULESS) < 0)
    goto out;

  rc = 0;
 out:
  hwloc_bitmap_free(rset);
  return rc;
}
