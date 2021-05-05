/******************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory
 ******************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_AMD_GPUS
#include "hip/hip_runtime.h"
#endif 

#define MAX_PCI_LEN 20

int main(int argc, char *argv[])
{
    char str[] = "1,7"; 

    // Copy VISDEVS string since strtok modifies the input string 
    char tmp[strlen(str)]; 
    strcpy(tmp, str);

    /* Get list size */ 
    int i, idevs = 0; 
    char *token = strtok(tmp, ",");
    while( token != NULL ) {
        idevs++; 
        token = strtok(NULL, ",");
    }

    /* Convert VISDEVS list into ints */ 
    //int i=0, visdevs[idevs];
    //strcpy(tmp, str);
    //token = strtok(tmp, ",");
    //while( token != NULL ) {
    //    visdevs[i++] = atoi(token);
    //    token = strtok(NULL, ",");
    //}

    // Don't invoke any GPU calls before resetting the environment!
    // Otherwise, there's no effect of setting VISIBLE_DEVICES. 
    //cudaGetDeviceCount(&ndevs);
    //printf("Initial num. devices %d\n", ndevs); 

    printf("Resetting environment to devices %s\n", str); 
    unsetenv("ROCR_VISIBLE_DEVICES");
    unsetenv("HIP_VISIBLE_DEVICES");
    unsetenv("CUDA_VISIBLE_DEVICES");
#ifdef HAVE_AMD_GPUS
    setenv("ROCR_VISIBLE_DEVICES", str, 1);
#else 
    setenv("CUDA_VISIBLE_DEVICES", str, 1);
#endif

    int odevs=-1; 
    cudaGetDeviceCount(&odevs);
    printf("Modified num. devices %d\n", odevs); 

    /* Get device PCI ID */ 
    char pci[MAX_PCI_LEN]; 
    for (i=0; i<idevs; i++) {
        pci[0] = '\0'; 
        cudaDeviceGetPCIBusId(pci, MAX_PCI_LEN, i);
        printf("PCI ID of device %d = %s\n", i, pci);
    }

    return 0; 
}