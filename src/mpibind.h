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
    
    /* The type of GPU by vendor */
    MPIBIND_GPU_AMD,    
    MPIBIND_GPU_NVIDIA,
  }; 
  
  /* Opaque mpibind handle */ 
  struct mpibind_t; 
  typedef struct mpibind_t mpibind_t;
    
  /* 
   * The mpibind API. 
   * Most calls return 0 on success and non-0 on failure. 
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
   * Output: The mapping policy.
   * Asssign CPUs, GPUs, and number of threads to each 
   * task/process in the job. 
   * These output functions should only be called after 
   * a call to the main 'mpibind' function. 
   */
  /*
   * Array with 'ntasks' elements. Each entry correspond 
   * to the number of threads to use for the process/task 
   * corresponding to this entry. 
   */ 
  int* mpibind_get_nthreads(mpibind_t *handle);
  /*
   * Array with 'ntasks' elements. The physical CPUs to 
   * use for a given process/task.
   */ 
  hwloc_bitmap_t* mpibind_get_cpus(mpibind_t *handle);
  /*
   * Array with 'ntasks' elements. The GPUs to use for a 
   * given process/task. 
   */
  hwloc_bitmap_t* mpibind_get_gpus(mpibind_t *handle);
  /*
   * The GPU vendor associated with the GPUs on this 
   * node: MPIBIND_GPU_AMD or MPIBIND_GPU_NVIDIA. 
   */
  int mpibind_get_gpu_type(mpibind_t *handle);
  /*
   * Get the hwloc loaded topology used by mpibind so that 
   * callers can continue to use it even after mpibind has
   * been finalized. This call, however, must be called 
   * before mpibind_finalize. This also means that the caller
   * is responsible for unloading/freeing the topology. 
   */
  hwloc_topology_t mpibind_get_topology(mpibind_t *handle); 

  /*
   * Helper functions 
   */
  /*
   * Print the mapping for each task. 
   */
  void mpibind_print_mapping(mpibind_t *handle);
   /*
   * Get the mapping for each task
   */
  int mpibind_get_mapping_string(mpibind_t *handle, char *mapping_buffer, size_t buffer_size);
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
  void mpibind_print_env_vars(mpibind_t *handle);  
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

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MPIBIND_H_INCLUDED */
