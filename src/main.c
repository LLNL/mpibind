/*
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory
 */ 
#include "mpibind.h"


/* 
 * Show the GPU mapping using various ID types. 
 */ 
void howto_gpu_ids(mpibind_t *handle)
{
  char **gpu_str; 
  char str[128];
  int i, j, k, ngpus;
  int ids[] = {MPIBIND_ID_PCIBUS, MPIBIND_ID_UNIV,
               MPIBIND_ID_VISDEVS, MPIBIND_ID_NAME}; 
  char *desc[] = { "PCI", "UUID", "VISDEVS", "NAME"};
  int ntasks = mpibind_get_ntasks(handle);

  for (k=0; k<sizeof(ids)/sizeof(int); k++) {
    /* Set the desired type of GPU ID */ 
    mpibind_set_gpu_ids(handle, ids[k]); 
    printf("GPU IDs using %s\n", desc[k]);
    /* Get the IDs per task */ 
    for (i=0; i<ntasks; i++) {
      gpu_str = mpibind_get_gpus_ptask(handle, i, &ngpus);
      if (ngpus > 0) {
        printf("\tTask %d: ", i);
        for (j=0; j<ngpus; j++) {
          printf("%s", gpu_str[j]);
          printf( (j == ngpus-1) ? "\n" : "," ); 
        }
      }
    }
  }

  /* For reference, show mpibind device IDs */ 
  hwloc_bitmap_t *gpus = mpibind_get_gpus(handle);
  printf("GPU IDs using mpibind numbering\n");
  for (i=0; i<ntasks; i++)
    if (hwloc_bitmap_list_snprintf(str, sizeof(str), gpus[i]) > 0)
      printf("\tTask %d: %s\n", i, str);  
}


/* 
 * Show how to extract and use certain environment 
 * variables related to affinity. 
 */ 
void howto_env_vars(mpibind_t *handle)
{
  /* Set the environment variables first */ 
  mpibind_set_env_vars(handle);

#if 0
  /* Take a comprehensive look */ 
  mpibind_env_vars_print(handle);
#else
  int i, nvars; 
  char **names;
  char **values; 
  int ntasks = mpibind_get_ntasks(handle);

  /* Get the names of the environment variables */ 
  names = mpibind_get_env_var_names(handle, &nvars);
  printf("Environment variables:\n");
  for (i=0; i<nvars; i++)
    printf("\t%s\n", names[i]);

  values = mpibind_get_env_var_values(handle, names[nvars-1]);
  printf("%s:\n", names[nvars-1]);
  for (i=0; i<ntasks; i++) {
    if (strlen(values[i]))
      printf("\t[%d]: %s\n", i, values[i]); 
  }
#endif
}



int main(int argc, char *argv[])
{
  /* Optional program input: number of tasks */ 
  int ntasks = 5; 
  if (argc > 1)
    ntasks = atoi(argv[1]); 

  mpibind_t *handle;
  mpibind_init(&handle);

  /* User input */
  mpibind_set_ntasks(handle, ntasks);
  //mpibind_set_nthreads(handle, 3);
  //mpibind_set_greedy(handle, 0);
  //mpibind_set_gpu_optim(handle, 0);
  //mpibind_set_smt(handle, 1);
  //params.restr_type = MEM; 
  //  mpibind_set_restrict_type(handle, MPIBIND_RESTRICT_CPU);
  //params.restr_set = "24-29,72-77,36-41,84-89";
  //params.restr_set = "24-35,72-83";
  //params.restr_set = "4-6";
  //params.restr_set = "8"; 
  //  mpibind_set_restrict_ids(handle, "24-35,72-83"); 

  /* Get the mapping */ 
  if ( mpibind(handle) )
    return 1;
  
  /* Get the hwloc topology to parse the hwloc_bitmaps */ 
  hwloc_topology_t topo; 
  topo = mpibind_get_topology(handle); 

  /* Verbose mapping */ 
  //Specify the type of GPU IDs to use
  mpibind_set_gpu_ids(handle, MPIBIND_ID_VISDEVS);
  mpibind_mapping_print(handle); 

  int ngpus = mpibind_get_num_gpus(handle);
  printf("There are %d GPUs\n", ngpus);
  if (ngpus > 0)
    /* Example using various GPU IDs */ 
    howto_gpu_ids(handle);
  
  /* Example using affinity environment variables */ 
  howto_env_vars(handle);
  
  /* Clean up */ 
  mpibind_finalize(handle);

  /* Last clean up activity: destroy the topology */ 
  hwloc_topology_destroy(topo);
  
  return 0;
}
