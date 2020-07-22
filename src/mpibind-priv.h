/******************************************************
 * Edgar A Leon
 * Lawrence Livermore National Laboratory 
 ******************************************************/
#ifndef MPIBIND_PRIV_H_INCLUDED
#define MPIBIND_PRIV_H_INCLUDED

#define SHORT_STR_SIZE 32
#define LONG_STR_SIZE 1024

#define VERBOSE 0
#define DEBUG 0


#define ERR_MSG(whatstr)						\
  do {									\
    fprintf(stderr, "%s failed: %s.\n", __func__, (whatstr));		\
  } while (0)


/* 
 * An environment variable with one value per task 
 */ 
typedef struct {
  int size; 
  char *name;
  char **values;
} mpibind_env_var;

/* 
 * The mpibind handle 
 */ 
struct mpibind_t {
  /* Input parameters */ 
  int ntasks;
  int in_nthreads;
  int greedy; 
  int gpu_optim; 
  int smt;
  char *restr_set;
  int restr_type;
  
  /* Input/Output parameters */ 
  hwloc_topology_t topo;
  
  /* Output parameters */
  int *nthreads;
  hwloc_bitmap_t *cpus; 
  hwloc_bitmap_t *gpus;
  int gpu_type;

  /* Environment variables */
  int nvars;
  char **names; 
  mpibind_env_var *env_vars; 
};


#endif // MPIBIND_PRIV_H_INCLUDED
