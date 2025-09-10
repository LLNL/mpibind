/***********************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory
 ***********************************************************/

#include <stdio.h>
#include <string.h>
#include <omp.h>
#include "affinity.h"

int main(int argc, char *argv[])
{
  char buf[LONG_STR_SIZE];
  int i;
  int ncpus = get_num_cpus();
  int verbose = 0;
  int nc = 0;

  /* Get rid of compiler warning. Ay. */
  (void) verbose;

  /* Command-line options */
  if (argc > 1)
    for (i=1; i<argc; i++) {
      if ( strcmp(argv[i], "-v") == 0 )
	verbose = 1;
    }

  nc += sprintf(buf+nc, "Process running on %d CPUs: ", ncpus);
  nc += get_cpu_affinity(buf+nc);
#ifdef HAVE_GPUS
  int ndevs = get_gpu_count();
  nc += sprintf(buf+nc, "Process has %d GPUs: ", ndevs);
  nc += get_gpu_affinity(buf+nc);
  nc += get_gpu_info_all(buf+nc);
#endif

  /* Print the process information */
  printf("\n%s", buf);

  /* Clear buffer for reuse */
  nc = 0;
  buf[0] = '\0';

#pragma omp parallel firstprivate(buf, nc) private(ncpus) shared(verbose)
  {
    int tid = omp_get_thread_num();
    int nthreads = omp_get_num_threads();
    ncpus = get_num_cpus();

    nc += sprintf(buf+nc, "Thread %3d/%3d running on %d CPUs: ",
		  tid, nthreads, ncpus);
    nc += get_cpu_affinity(buf+nc);
#ifdef HAVE_GPUS
    int dev = tid % ndevs;
    nc += sprintf(buf+nc, "Thread %3d/%3d assigned to GPU: 0x%x\n",
		  tid, nthreads, get_gpu_pci_id(dev));
    if (verbose)
      nc += get_gpu_info(dev, buf+nc);
#endif

    printf("%s", buf);
  }

  return 0;
}
