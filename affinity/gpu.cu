/***********************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory 
 ***********************************************************/

#include <stdio.h>
#include <cuda_runtime.h>      /* Documentation in hip_runtime_api.h */ 
#include "affinity.h"          /* Do not perform name mangling */ 


int get_gpu_count()
{
  /* 
     Surprinsingly, I must set 'count' to zero before
     passing it to cudaGetDeviceCount(&count)
     If CUDA_VISIBLE_DEVICES is set to '', calling 
     this function will not set a value for count. 
     Then, count will be used uninitialized and 
     most likely the program will segfault. 
  */ 
  int count=0;

  if (cudaSuccess != cudaGetDeviceCount(&count))
    fprintf(stderr, "GetDeviceCount failed\n");

  return count;
}


int get_gpu_pci_id(int dev)
{
  int value = -1; 
  cudaError_t err = cudaDeviceGetAttribute(&value, cudaDevAttrPciBusId, dev);
  
  if ( err )
    fprintf(stderr, "Could not get PCI ID for GPU %d\n", dev);

  return value; 
}


int get_gpu_affinity(char *buf)
{
  int count=0;

  if (cudaSuccess != cudaGetDeviceCount(&count))
    fprintf(stderr, "GetDeviceCount failed\n");
  
  int nc=0; 
  int i; 
  for (i=0; i<count; i++) {
#if 0
    cudaDeviceProp prop;
    cudaError_t err = cudaGetDeviceProperties(&prop, i);
    if ( err ) {
      fprintf(stderr, "Could not get info for GPU %d\n", i);
      return -1;
    }
    nc += sprintf(buf+nc, "%04x:%02x ", prop.pciDomainID, prop.pciBusID);
#else
    // [domain]:[bus]:[device].[function]
    char pcibusid[SHORT_STR_SIZE];
    cudaError_t err = cudaDeviceGetPCIBusId(pcibusid, sizeof(pcibusid), i);
    if ( err ) {
      fprintf(stderr, "Get device PCI Bus ID failed");
      return -1;
    }
    nc += sprintf(buf+nc, "%s ", pcibusid);
#endif
  }
  nc += sprintf(buf+nc, "\n"); 
  
  return nc; 
}


int get_gpu_info(int devid, char *buf)
{
  cudaDeviceProp prop;
  cudaError_t err; 
  int nc = 0;
  
  err = cudaGetDeviceProperties(&prop, devid);
  if ( err ) {
    fprintf(stderr, "Could not get info for GPU %d\n", devid);
    return -1;
  }

  float ghz = prop.clockRate / 1000.0 / 1000.0; 
#if 1
  nc += sprintf(buf+nc, "\tName: %s\n", prop.name);
  nc += sprintf(buf+nc, "\tPCI domain ID 0x%x\n", prop.pciDomainID);
  nc += sprintf(buf+nc, "\tPCI bus ID: 0x%x\n", prop.pciBusID);
  nc += sprintf(buf+nc, "\tPCI device ID 0x%x\n", prop.pciDeviceID);
  nc += sprintf(buf+nc, "\tMemory: %lu GB\n", prop.totalGlobalMem >> 30);
  nc += sprintf(buf+nc, "\tMultiprocessor count: %d\n", prop.multiProcessorCount);
  nc += sprintf(buf+nc, "\tClock rate: %.3f Ghz\n", ghz); 
  nc += sprintf(buf+nc, "\tCompute capability: %d.%d\n",
		prop.major, prop.minor);
  nc += sprintf(buf+nc, "\tECC enabled: %d\n", prop.ECCEnabled);
#else
  nc += sprintf(buf+nc, "\t0x%.2x: %s, %lu GB Mem, "
		"%d Multiprocessors, %.3f GHZ, %d.%d CC\n",
		prop.pciBusID, prop.name, prop.totalGlobalMem >> 30,
		prop.multiProcessorCount, ghz, prop.major, prop.minor); 
#endif
  
  return nc; 
}


int get_gpu_info_all(char *buf)
{
  cudaError_t err; 
  int i, myid, count=0;
  int nc=0; 

  err = cudaGetDeviceCount(&count);
  if ( err ) {
    fprintf(stderr, "Get device count failed\n");
    return -1;
  }

  err = cudaGetDevice(&myid);
  if ( err ) {
    fprintf(stderr, "Get default device failed\n");
    return -1; 
  }

  char pcibusid[SHORT_STR_SIZE];
  err = cudaDeviceGetPCIBusId(pcibusid, sizeof(pcibusid), myid);
  if ( err ) {
    fprintf(stderr, "Get device PCI Bus ID failed");
    return -1;
  }

  nc += sprintf(buf+nc, "\tDefault device: %s\n", pcibusid);
  
  for (i=0; i<count; i++) {
    //nc += sprintf(buf+nc, "\t--\n"); 
    nc += get_gpu_info(i, buf+nc);
  }
  
  return nc; 
}




