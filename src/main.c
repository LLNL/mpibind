/*
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory
 */ 
#include "mpibind.h"


int main(int argc, char *argv[])
{
  mpibind_t *handle;
  hwloc_topology_t topo; 

  mpibind_init(&handle);

  /* User input */
  int ntasks = 5; 
  mpibind_set_ntasks(handle, ntasks);
  //mpibind_set_nthreads(handle, 3);
  mpibind_set_greedy(handle, 0);
  mpibind_set_gpu_optim(handle, 0);
  //mpibind_set_smt(handle, 4);
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
  
  /* Get the topology so I can use it to parse the hwloc_bitmaps */ 
  topo = mpibind_get_topology(handle); 
  
  /* Verbose mapping */ 
  mpibind_print_mapping(handle); 

  /* Play with the env vars I would set */ 
  mpibind_set_env_vars(handle);

  /* Take a quick look */ 
  //mpibind_print_env_vars(handle);

  /* Extract the values of the env vars */ 
  int i, nvars; 
  char **var_names = mpibind_get_env_var_names(handle, &nvars);
  char **values = mpibind_get_env_var_values(handle, var_names[nvars-1]);
  for (i=0; i<nvars; i++)
    printf("var[%d]: %s\n", i, var_names[i]);
  for (i=0; i<ntasks; i++)
    printf("%s[%d]: %s\n", var_names[nvars-1], i, values[i]); 
  
  /* Clean up */ 
  mpibind_finalize(handle);

  /* Don't forget to unload/free the topology */ 
  hwloc_topology_destroy(topo);
  
  return 0;
}
