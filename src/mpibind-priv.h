/******************************************************
 * Edgar A Leon
 * Lawrence Livermore National Laboratory 
 ******************************************************/
#ifndef MPIBIND_PRIV_H_INCLUDED
#define MPIBIND_PRIV_H_INCLUDED

#define SHORT_STR_SIZE 32
#define LONG_STR_SIZE 1024

#define PCI_BUSID_LEN 16
#define UUID_LEN 64
#define MAX_IO_DEVICES 128
#define MAX_CPUS_PER_TASK 1024

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
 * The type of I/O devices 
 */ 
enum { 
    DEV_GPU,    
    DEV_NIC,
}; 

/*
 * The various I/O device IDs. 
 * GPU devices are different from other I/O devices 
 * by having visdevs (and optionally smi) set 
 * to a non-negative integer. 
 */
struct device {
  char name[SHORT_STR_SIZE]; // Device name 
  char pci[PCI_BUSID_LEN];   // PCI bus ID
  char univ[UUID_LEN];       // Universally unique ID 
  hwloc_uint64_t ancestor;   // Smallest non-I/O ancestor (gp_index)
  int type;                  // Type of I/O device, e.g., DEV_GPU
  int vendor;                // Device vendor
  /* GPU specific */ 
  int smi;                   // System management ID (RSMI and NVML)
  int visdevs;               // CUDA/ROCR visible devices ID
}; 

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
  char ***gpus_usr; 
  int **cpus_usr;

  /* Environment variables */
  int nvars;
  char **names; 
  mpibind_env_var *env_vars; 

  /* IDs of I/O devices */
  int ndevs;  
  struct device **devs;
};


#endif // MPIBIND_PRIV_H_INCLUDED
