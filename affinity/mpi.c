/***********************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory 
 ***********************************************************/

#include <stdio.h>
#include <string.h>
#include <mpi.h>
#include "affinity.h"


int main(int argc, char *argv[])
{
  char buf[LONG_STR_SIZE];
  char hostname[MPI_MAX_PROCESSOR_NAME]; 
  int rank, np, size, i;
  int verbose = 0; 
  int ncpus = get_num_cpus(); 
  int nc = 0; 

  /* Get rid of compiler warning. Ay. */
  (void) verbose; 
  
  /* Command-line options */
  if (argc > 1) 
    for (i=1; i<argc; i++) {
      if ( strcmp(argv[i], "-v") == 0 )
	verbose = 1; 
    }
  
  MPI_Init(&argc, &argv); 
  MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
  MPI_Comm_size(MPI_COMM_WORLD, &np); 
  MPI_Get_processor_name(hostname, &size); 

  nc += sprintf(buf+nc, "%-10s Task %3d/%3d running on %d CPUs: ",
		hostname, rank, np, ncpus);
  nc += get_cpu_affinity(buf+nc);
#ifdef HAVE_GPUS
  int ndevs = get_gpu_count();
  nc += sprintf(buf+nc, "%10s Task %3d/%3d has %d GPUs: ",
		"", rank, np, ndevs); 
  nc += get_gpu_affinity(buf+nc);
  if (verbose)
    nc += get_gpu_info_all(buf+nc);
#endif
  
  /* Print per-task information */ 
  printf("%s", buf);
  
  MPI_Finalize(); 

  return 0; 
}
