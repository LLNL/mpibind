#include <hwloc.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
    int nthreads, tid;

#pragma omp parallel private(nthreads, tid)
    {
        /* Obtain thread number */
        tid = omp_get_thread_num ();
        printf ("Hello World from thread = %d\n", tid);

        if (tid == 0) {
            nthreads = omp_get_num_threads ();
            printf ("Number of threads = %d\n", nthreads);

            omp_proc_bind_t pb = omp_get_proc_bind();
            printf("BIND = %d\n", pb);
        }
    }
    return 0;
}
