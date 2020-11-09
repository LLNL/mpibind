/***********************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory 
 ***********************************************************/

#include <stdio.h>
#include <string.h>
#include <mpi.h>
#include <omp.h>
#include "affinity.h"


static
void usage(char *name)
{
  printf("Usage: %s [options]\n", name);
  printf("\t    -mpi: Show MPI info only (no OpenMP)\n"); 
  printf("\t-verbose: Show detailed GPU info when -mpi enabled\n");
  printf("\t   -help: Show this page\n");
}


int main(int argc, char *argv[])
{
  char buf[LONG_STR_SIZE];
  char hostname[MPI_MAX_PROCESSOR_NAME]; 
  int rank, np, size, i, ngpus, ncpus;
  int verbose = 0;
  int help = 0; 
  int mpi = 0; 
  int nc = 0; 
  
  /* Command-line options */
  if (argc > 1) 
    for (i=1; i<argc; i++) {
      /* Todo: Eat heading dashes here */ 
      if ( strcmp(argv[i], "-v") >= 0 )
	verbose = 1;
      else if ( strcmp(argv[i], "-m") >= 0 )
	mpi = 1;
      else if ( strcmp(argv[i], "-h") >= 0 )
	help = 1; 
    }
  
  MPI_Init(&argc, &argv); 
  MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
  MPI_Comm_size(MPI_COMM_WORLD, &np); 
  MPI_Get_processor_name(hostname, &size); 
  
  if (help) {
    if (rank == 0)
      usage(argv[0]);
    
    MPI_Finalize(); 
    return 0; 
  }
  
  if ( mpi ) {
    
    /* MPI */  
    ncpus = get_num_cpus();
    nc += sprintf(buf+nc, "%s Task %2d/%2d with %d cpus: ",
		  hostname, rank, np, ncpus);
    nc += get_cpu_affinity(buf+nc);
#ifdef HAVE_GPUS
    ngpus = get_gpu_count(); 
    nc += sprintf(buf+nc, "%s Task %2d/%2d with %d gpus: ",
		  hostname, rank, np, ngpus); 
    nc += get_gpu_affinity(buf+nc);
    if (verbose)
      nc += get_gpu_info_all(buf+nc);
#endif
    
    /* Print per-task information */ 
    printf("%s", buf);
    
  } else {

    /* MPI+OpenMP */ 
#ifdef HAVE_GPUS    
    ngpus = get_gpu_count();
#endif 
    
#pragma omp parallel firstprivate(buf, nc) private(ncpus) shared(rank, np, ngpus, verbose)
    {
      int tid = omp_get_thread_num();
      int nthreads = omp_get_num_threads();
      ncpus = get_num_cpus();
      
      nc += sprintf(buf+nc, "%s Task %3d/%3d Thread %3d/%3d with %2d cpus: ",
		    hostname, rank, np, tid, nthreads, ncpus);
      nc += get_cpu_affinity(buf+nc);
#ifdef HAVE_GPUS
      nc += sprintf(buf+nc, "%s Task %3d/%3d Thread %3d/%3d with %2d gpus: ",
		    hostname, rank, np, tid, nthreads, ngpus);
      nc += get_gpu_affinity(buf+nc);
#endif
      
      /* Print per-worker information */ 
      printf("%s", buf);
    }
    
  }
  
  MPI_Finalize(); 
  return 0; 
}
