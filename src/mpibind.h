/******************************************************
 * Edgar A Leon
 * Lawrence Livermore National Laboratory
 ******************************************************/
#ifndef MPIBIND_H_INCLUDED
#define MPIBIND_H_INCLUDED

#include <hwloc.h>

#ifdef __cplusplus
extern "C" {
#endif

  enum {
    /* Caller can restrict the topology based on CPU or MEM */
    MPIBIND_RESTRICT_CPU,
    MPIBIND_RESTRICT_MEM,

    /* Type of I/O ID */
    MPIBIND_ID_UNIV,
    MPIBIND_ID_SMI,
    MPIBIND_ID_PCIBUS,
    MPIBIND_ID_NAME,
  };

  /* Opaque mpibind handle */
  struct mpibind_t;
  typedef struct mpibind_t mpibind_t;

  /*
   * The mpibind API.
   * Most calls return zero on success and non-zero on failure.
   */

  /*
   * Initialize an mpibind handle. All mpibind functions require
   * this function to be called first.
   */
  int mpibind_init(mpibind_t **handle);

  /*
   * Release resources associated with the input handle.
   * After this call, no mpibind function should be called.
   */
  int mpibind_finalize(mpibind_t *handle);

  /*
   * Required input parameters
   */

  /*
   * The number of processes/tasks in the job.
   */
  int mpibind_set_ntasks(mpibind_t *handle,
	      int ntasks);

  /*
   * Optional input parameters
   */

  /*
   * The number of threads per process.
   * If nthreads is not provided, mpibind calculates an
   * appropriate number of threads for each task
   * (see mpibind_get_nthreads).
   */
  int mpibind_set_nthreads(mpibind_t *handle,
	      int nthreads);

  /*
   * Valid values are 0 and 1. Default is 1.
   * If 1 and the number of tasks is less than the number of
   * NUMA domains, mpibind assigns all of the available hardware
   * resources to the input tasks.
   * Otherwise, mpibind assigns no more than one NUMA domain
   * (and associated resources) per task.
   */
  int mpibind_set_greedy(mpibind_t *handle,
	      int greedy);

  /*
   * Valid values are 0 and 1. Default is 1.
   * If 1, optimize task placement for GPUs.
   * Otherwise, optimize placement for CPUs.
   */
  int mpibind_set_gpu_optim(mpibind_t *handle,
			  int gpu_optim);

  /*
   * Map the application workers to this SMT-level.
   * For an n-way SMT architecture, valid values are 1 to n.
   * If not specified, mpibind determines the SMT level
   * appropriately based on the number of input workers.
   */
  int mpibind_set_smt(mpibind_t *handle,
		    int smt);

  /*
   * Restrict the hardware topology to resources
   * associated with the specified hardware ids of type 'restr_type'.
   */
  int mpibind_set_restrict_ids(mpibind_t *handle,
			  char *restr_set);

  /*
   * Specify the type of resource to use to restrict
   * the hardware topology: MPIBIND_RESTRICT_CPU or
   * MPIBIND_RESTRICT_MEM.
   * The default value is MPIBIND_RESTRICT_CPU and it only
   * applies if 'restr_set' is given.
   */
  int mpibind_set_restrict_type(mpibind_t *handle,
				int restr_type);

  /*
   * Pass a loaded topology to mpibind.
   * mpibind will use this topology to perform the mappings
   * instead of loading its own topology.
   * Call this function after mpibind_init() and
   * before mpibind().
   */
  int mpibind_set_topology(mpibind_t *handle,
			   hwloc_topology_t topo);

  /*
   * Main mapping function.
   * The resulting mapping can be retrieved with the
   * mpibind_get* functions below.
   */
  int mpibind(mpibind_t *handle);

  /*
   * Output: The mapping of workers to the hardware.
   * Asssign CPUs, GPUs, and number of threads to each
   * task/process in the job.
   * These output functions should only be called after
   * mpibind().
   */

  /*
   * Return an array with 'ntasks' elements.
   * Each entry correspond to the number of threads to use
   * for the process/task corresponding to this entry.
   */
  int* mpibind_get_nthreads(mpibind_t *handle);

  /*
   * Return an array with 'ntasks' elements.
   * The physical CPUs to use for a given process/task.
   */
  hwloc_bitmap_t* mpibind_get_cpus(mpibind_t *handle);

  /*
   * Return an array with the CPUs assigned to the
   * given task. The size of the array is set in 'ncpus'.
   */
  int* mpibind_get_cpus_ptask(mpibind_t *handle, int taskid,
			      int *ncpus);

  /*
   * Return an array with the GPUs assigned to the
   * given task. The size of the array is set in 'ngpus'.
   */
  char** mpibind_get_gpus_ptask(mpibind_t *handle,
          int taskid, int *ngpus);

  /*
   * Get the number of GPUs in the system/allocation.
   */
  int mpibind_get_num_gpus(mpibind_t *handle);

  /*
   * Return an array with 'ntasks' elements.
   * The GPUs to use for a given process/task.
   * The GPU IDs use mpibind's device IDs
   * (as opposed to VISIBLE_DEVICES IDs).
   */
  hwloc_bitmap_t* mpibind_get_gpus(mpibind_t *handle);

  /*
   * Get the hwloc loaded topology used by mpibind so that
   * callers can continue to use it even after mpibind has
   * been finalized. This call, however, must be called
   * before mpibind_finalize. This also means that the caller
   * is responsible for destroying the topology.
   */
  hwloc_topology_t mpibind_get_topology(mpibind_t *handle);

  /*
   * Helper functions
   */

  /*
   * Print the mapping for each task.
   */
  void mpibind_mapping_print(mpibind_t *handle);

  /*
   * Print the mapping for a given task.
   */
  int mpibind_mapping_ptask_snprint(char *buf, size_t size,
        mpibind_t *handle, int taskid);

  /*
   * Print the mapping for each task to a string.
   * Returns the number of characters printed to the string.
   */
  int mpibind_mapping_snprint(char *str, size_t size,
        mpibind_t *handle);

  /*
   * Set the type of GPU IDs.
   * The different types are above and include
   * MPIBIND_ID_PCI and MPIBIND_ID_VISDEVS.
   * Call this function before mpibind_get_gpus_ptask() or
   * mpibind_mapping*print() to get the updated IDs.
   */
  int mpibind_set_gpu_ids(mpibind_t *handle,
        int id_type);

  /*
   * Environment variables that need to be exported by the runtime.
   * CUDA_VISIBLE_DEVICES --comma separated
   * ROCR_VISIBLE_DEVICES
   * OMP_NUM_THREADS
   * OMP_PLACES --comma separated, each item in curly braces.
   * OMP_PROC_BIND --spread
   * It is the caller's responsibility to export the environment
   * variables as desired. This function simply stores them and
   * the appropriate values for the caller.
   */
  int mpibind_set_env_vars(mpibind_t *handle);

  /*
   * Print the environment variables.
   * mpibind_set_env_vars must be called before this function
   * is executed.
   */
  void mpibind_env_vars_print(mpibind_t *handle);

  /*
   * For each task, return the value of a given env
   * variable (name). The output array has size 'ntasks'.
   * mpibind_set_env_vars must be called before this function
   * is executed.
   */
  char** mpibind_get_env_var_values(mpibind_t *handle,
				    char *name);

  /*
   * Return an array with the names of the env variables.
   * The size of the array is given in *count.
   * mpibind_set_env_vars must be called before this function
   * is executed.
   */
  char** mpibind_get_env_var_names(mpibind_t *handle, int *count);

  /*
   * Other "getter" functions.
   * These may be used to retrieve mpibind parameters
   * in a different context than that where they were
   * originally set.
   */

  /*
   * Get the number of tasks associated with an
   * mpibind handle.
   */
  int mpibind_get_ntasks(mpibind_t *handle);

  /*
   * Get whether or not an mpibind handle has
   * been set to bind resources greedily.
   */
  int mpibind_get_greedy(mpibind_t *handle);

  /*
   * Get whether or not gpu optimization has been
   * specified on an mpibind handle.
   */
  int mpibind_get_gpu_optim(mpibind_t *handle);

  /*
   * Get the given smt setting associated with an
   * mpibind handle.
   */
  int mpibind_get_smt(mpibind_t *handle);

  /*
   * Get the restrict id set associated with an
   * mpibind handle.
   */
  char* mpibind_get_restrict_ids(mpibind_t *handle);

  /*
   * Get the restrict type associated with an
   * mpibind handle.
   */
  int mpibind_get_restrict_type(mpibind_t *handle);

  /*
   * Apply mpibind affinity for task `taskid`
   */
  int mpibind_apply(mpibind_t *handle, int taskid);

  /*
   * Get the hwloc depth of a Core object.
   * mpibind relies on hwloc Core objects. If the topology
   * doesn't have them, use an appropriate replacement.
   * Make sure to always use mpibind_get_core_type and
   * mpibind_get_core_depth instead of HWLOC_OBJ_CORE
   */
  int mpibind_get_core_depth(hwloc_topology_t topo);

  /*
   * Get the hwloc type of a Core object
   */
  hwloc_obj_type_t mpibind_get_core_type(hwloc_topology_t topo);

  /*
   * Pop CPUs from the assigned CPUs of a task.
   */
  int mpibind_pop_cpus_ptask(mpibind_t *handle, int taskid, int ncpus);

  /*
   * Pop cores from the assigned CPUs of a task.
   */
  int mpibind_pop_cores_ptask(mpibind_t *handle, int taskid, int ncores);

  /*
   * Restrict the topology to the current binding.
   */
  int mpibind_restrict_to_current_binding(hwloc_topology_t topo);

  /*
   * Get the number of ints within a range
   */
  int mpibind_range_nints(char *arg);

  /*
   * Read in the restrict IDs from a string or a file
   */
  int mpibind_parse_restrict_ids(char *restr, int len);

  /*
   * Parse resource manager plugin options
   */
  char* mpibind_parse_option(const char *opt,
			     int *debug, int *gpu, int *greedy,
			     int *master,
			     int *omp_places, int *omp_proc_bind,
			     int *smt, int *turn_on,
			     int *verbose, int *visdevs);

  /*
   * Get the PUs associated with a given set of Cores
   */
  int mpibind_cores_to_pus(hwloc_topology_t topo, char *cores,
			   char *pus, int pus_str_size);

  /*
   * Get hwloc version
   */
  void mpibind_get_hwloc_version(char *ver);

  /*
   * Wrapper for hwloc_topology_load with mpibind's requirements
   *
   * When using the caller's topology, make sure that important
   * components are enabled such as PCI devices.
   * This function should be called after hwloc_topology_init
   */
  int mpibind_load_topology(hwloc_topology_t topo);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MPIBIND_H_INCLUDED */
