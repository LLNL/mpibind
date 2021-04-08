/***********************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory 
 ***********************************************************/

#include <stdio.h>
#include <hip/hip_runtime.h>
#include <mpi.h>

#define STR_SIZE 100

void check_devices(char *buf)
{
  hipDevice_t mydev;
  hipDeviceProp_t devProp;
  int i, ndevs, myid;
  char pciBusId[STR_SIZE] = "";
  int nc = 0; 
  
  hipGetDeviceCount(&ndevs);
  nc += sprintf(buf+nc, "Num devices: %d\n", ndevs); 

  hipGetDevice(&myid); 
  hipDeviceGet(&mydev, myid);
  hipDeviceGetPCIBusId(pciBusId, STR_SIZE, mydev);
  nc += sprintf(buf+nc, "Default device: %s\n", pciBusId); 
		
  for (i=0; i<ndevs; i++) { 
    hipGetDeviceProperties(&devProp, i);
    nc += sprintf(buf+nc, "\t--\n"); 
    nc += sprintf(buf+nc, "\tSystem major and minor: %d %d\n",
		  devProp.major, devProp.minor); 
    nc += sprintf(buf+nc, "\tName: %s\n", devProp.name); 
    nc += sprintf(buf+nc, "\tPCI bus ID: 0x%x\n", devProp.pciBusID);
    nc += sprintf(buf+nc, "\tPCI device ID 0x%x\n", devProp.pciDeviceID);
  }
}


int main(int argc, char *argv[])
{
  int rank, np;
  char buf[1024]; 
  
  MPI_Init(&argc, &argv); 
  MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
  MPI_Comm_size(MPI_COMM_WORLD, &np); 
  
  check_devices(buf);

  printf("Buf: %s\n", buf);
  
  MPI_Finalize(); 

  return 0; 
}
