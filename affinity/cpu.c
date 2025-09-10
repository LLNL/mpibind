/***********************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory
 ***********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* __USE_GNU is needed for CPU_ISSET definition */
#ifndef __USE_GNU
#define __USE_GNU 1
#endif
#include <sched.h>            // sched_getaffinity

/*
 * Convert a non-negative array of ints to a range
 */
int int2range(int *intarr, int size, char *range)
{
  int i, curr;
  int nc = 0;
  int start = -1;
  int prev = -2;

  for (i=0; i<size; i++) {
    curr = intarr[i];
    if (curr != prev+1) {
      /* Record end of range */
      if (start != prev && prev >= 0)
	nc += sprintf(range+nc, "-%d", prev);

      /* Record start of range */
      if (prev >= 0)
	nc += sprintf(range+nc, ",");
      nc += sprintf(range+nc, "%d", curr);
      start = curr;
    } else
      /* The last int is end of range */
      if (i == size-1)
	nc += sprintf(range+nc, "-%d", curr);

    prev = curr;
  }

  return nc;
}

/*
 * Get number of processing units (cores or hwthreads)
 */
static
int get_total_num_pus()
{
  int pus = sysconf(_SC_NPROCESSORS_ONLN);

  if ( pus < 0 )
    perror("sysconf");

  return pus;
}

/*
 * Get the affinity.
 */
static
int get_affinity(int *cpus, int *count)
{
  int i;
  cpu_set_t resmask;

  CPU_ZERO(&resmask);

  int rc = sched_getaffinity(0, sizeof(resmask), &resmask);
  if ( rc < 0 ) {
    perror("sched_getaffinity");
    return rc;
  }

  *count = 0;
  int pus = get_total_num_pus();
  for (i=0; i<pus; i++)
    if ( CPU_ISSET(i, &resmask) ) {
      cpus[*count] = i;
      (*count)++;
    }

  return 0;
}

/*
 * Get the number of CPUs where this worker can run.
 */
int get_num_cpus()
{
  cpu_set_t mask;

  CPU_ZERO(&mask);

  int rc = sched_getaffinity(0, sizeof(mask), &mask);
  if ( rc < 0 ) {
    perror("sched_getaffinity");
    return rc;
  }

  return CPU_COUNT(&mask);
}

/*
 * Print my affinity into a buffer.
 */
int get_cpu_affinity(char *outbuf)
{
  int count;
  int nc = 0;

  int *cpus = malloc(sizeof(int) * get_total_num_pus());
  get_affinity(cpus, &count);

#if 1
  nc += int2range(cpus, count, outbuf+nc);
  //printf("nc=%d count=%d\n", nc, count);
#else
  int i;
  for (i=0; i<count; i++) {
    nc += sprintf(outbuf+nc, "%d ", cpus[i]);
  }
#endif
  nc += sprintf(outbuf+nc, "\n");

  free(cpus);

  return nc;
}
